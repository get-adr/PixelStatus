# Einmaliges Helper-Skript: legt die Microsoft-Entra-App-Registrierung an, die
# die PixelStatus-Companion-App fuer die Teams-Praesenz braucht (Public Client,
# delegierte Berechtigung Presence.Read). Die Anmeldung laeuft im Systembrowser.
#
#   pwsh ./New-PixelStatusAppRegistration.ps1                 # Standard-Name
#   pwsh ./New-PixelStatusAppRegistration.ps1 -InstallModules # fehlende Module installieren
#   pwsh ./New-PixelStatusAppRegistration.ps1 -DisplayName "PixelStatus" -Tenant "<tenant-id>"
#
# Am Ende gibt es die Client (Application) ID fuer die Companion-App aus.
[CmdletBinding()]
param(
  [string]$DisplayName = "PixelStatus",
  [string]$Tenant = "",        # leer = Anmeldetenant des Benutzers
  # Muss exakt zur Companion-App passen (fester Loopback-Port).
  [string]$RedirectUri = "http://127.0.0.1:8939/callback",
  [switch]$InstallModules
)

$ErrorActionPreference = "Stop"

# -- Benoetigte Graph-PowerShell-Module -------------------------------------
$requiredModules = @(
  "Microsoft.Graph.Authentication",
  "Microsoft.Graph.Applications"
)
$missing = @($requiredModules | Where-Object { -not (Get-Module -ListAvailable -Name $_) })
if ($missing.Count -gt 0) {
  if ($InstallModules) {
    Write-Host "Installiere fehlende Module: $($missing -join ', ')" -ForegroundColor Yellow
    Install-Module -Name $missing -Scope CurrentUser -Force -AllowClobber -SkipPublisherCheck -Confirm:$false
  } else {
    Write-Error ("Fehlende PowerShell-Module: {0}. Installieren mit: " +
      "Install-Module -Name {0} -Scope CurrentUser    (oder -InstallModules).") -f ($missing -join ', ')
    exit 1
  }
}
Import-Module -Name @("Microsoft.Graph.Authentication", "Microsoft.Graph.Applications")

# -- Anmeldung (Browser, Out-of-Band) ---------------------------------------
Write-Host "Anmeldung bei Microsoft Entra -- es oeffnet sich ein Browserfenster." -ForegroundColor Cyan
# Microsoft Graph bietet fuer diese delegierten Schreiboperationen keinen
# Application.ReadWrite.OwnedBy-Scope. Application.ReadWrite.All ist daher der
# engste nutzbare delegierte Scope; ein Service-Principal-Zugriff wird nicht
# angefordert.
$connectParams = @{
  Scopes = @("Application.ReadWrite.All")
}
if ($Tenant) { $connectParams["TenantId"] = $Tenant }
Connect-MgGraph @connectParams

# App-Registrierung: vorhandene uebernehmen statt eine Duplikat zu erzeugen.
$escapedDisplayName = $DisplayName.Replace("'", "''")
$app = Get-MgApplication -Filter "displayName eq '$escapedDisplayName'" |
  Select-Object -First 1
if ($app) {
  Write-Warning ("Es existiert bereits eine App '$DisplayName' (AppId {0}) -- wird uebernommen." -f $app.AppId)
} else {
  $app = New-MgApplication -DisplayName $DisplayName -SignInAudience "AzureADMultipleOrgs"
  Write-Host ("App-Registrierung angelegt: '{0}' (AppId {1})" -f $app.DisplayName, $app.AppId) -ForegroundColor Green
}

# Die Microsoft-Graph-App-ID und Presence.Read-Scope-ID sind stabile IDs. Die
# feste Scope-ID vermeidet Application.Read.All nur zum Lesen des Graph-SP.
$graphSpAppId = "00000003-0000-0000-c000-000000000000"
$presenceReadScopeId = "76bc735e-aecd-4a1d-8b4c-2b915deabb79"

# requiredResourceAccess ist ein Replace-Feld: andere API-Ressourcen bleiben
# erhalten; fuer Microsoft Graph wird nur Presence.Read eingetragen.
$rra = @($app.RequiredResourceAccess | Where-Object { $_.ResourceAppId -ne $graphSpAppId })
$rra += [ordered]@{
  resourceAppId  = $graphSpAppId
  resourceAccess = @([ordered]@{ id = $presenceReadScopeId; type = "Scope" })
}

# Nur die native Redirect-Plattform setzen. Vorhandene Web-Redirects und
# Zugangsdaten bleiben unangetastet; sie sind fuer den PKCE-Flow nicht noetig.
Update-MgApplication -ApplicationId $app.Id -ErrorAction Stop -BodyParameter @{
  requiredResourceAccess = $rra
  publicClient           = @{ redirectUris = @($RedirectUri) }
}

# Public-Client-Fallback fuer den nativen PKCE-Flow aktivieren.
Update-MgApplication -ApplicationId $app.Id -IsFallbackPublicClient $true -ErrorAction Stop

# Nachpruefung: die exakte Loopback-URI muss als Public-Client-Redirect stehen.
$app2 = Get-MgApplication -ApplicationId $app.Id
if (@($app2.PublicClient.RedirectUris) -notcontains $RedirectUri) {
  Write-Error "publicClient.redirectUris enthaelt die Loopback nicht (ist: '$(@($app2.PublicClient.RedirectUris) -join ', ')') -- App ist nicht korrekt konfiguriert."
  exit 1
}
Write-Host "Delegierte Berechtigung 'Presence.Read' + Public-Client-Redirect '$RedirectUri' gesetzt." -ForegroundColor Green

Write-Host ""
Write-Host "Fertig. Client (Application) ID fuer die Companion-App:" -ForegroundColor Cyan
Write-Host ("  {0}" -f $app.AppId)
Write-Host ""
Write-Host "In die Companion-App unter Einstellungen -> 'Entra-Client-ID' eintragen."
