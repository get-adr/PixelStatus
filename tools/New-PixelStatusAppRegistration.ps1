# Einmaliges Helper-Skript: legt die Microsoft-Entra-App-Registrierung an, die
# die PixelStatus-Companion-App fuer die Teams-Präsenz braucht (Public Client,
# delegierte Berechtigung Presence.Read). Die Anmeldung laeuft im Systembrowser
# (Out-of-Band), weil der Device-Code-Flow in vielen Tenants gesperrt ist --
# fuer dieses einmalige Einrichten laeuft das aber problemlos.
#
#   pwsh ./New-PixelStatusAppRegistration.ps1                 # Standard-Name
#   pwsh ./New-PixelStatusAppRegistration.ps1 -InstallModules # fehlende Module installieren
#   pwsh ./New-PixelStatusAppRegistration.ps1 -DisplayName "PixelStatus" -Tenant "<tenant-id>"
#
# Am Ende gibt es die Client (Application) ID aus, die in die Companion-App
# (Einstellungen -> Entra-Client-ID) eingetragen wird.
[CmdletBinding()]
param(
  [string]$DisplayName = "PixelStatus",
  [string]$Tenant = "",        # leer = Anmeldetenant des Benutzers
  [switch]$InstallModules
)

$ErrorActionPreference = "Stop"

# -- Benoetigte Graph-PowerShell-Module -------------------------------------
$requiredModules = @(
  "Microsoft.Graph.Authentication",
  "Microsoft.Graph.Applications",
  "Microsoft.Graph.Identity.DirectoryManagement"
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
Import-Module -Name @("Microsoft.Graph.Authentication", "Microsoft.Graph.Applications",
                      "Microsoft.Graph.Identity.DirectoryManagement")

# -- Anmeldung (Browser, Out-of-Band) ---------------------------------------
Write-Host "Anmeldung bei Microsoft Entra -- es oeffnet sich ein Browserfenster." -ForegroundColor Cyan
$connectParams = @{
  Scopes = @("Application.ReadWrite.Owned")
}
if ($Tenant) { $connectParams["TenantId"] = $Tenant }
Connect-MgGraph @connectParams

# App-Registrierung: vorhandene uebernehmen statt eine Duplikat zu erzeugen.
$app = Get-MgApplication -Filter "displayName eq '$DisplayName'" -ErrorAction SilentlyContinue |
  Select-Object -First 1
if ($app) {
  Write-Warning ("Es existiert bereits eine App '$DisplayName' (AppId {0}) -- wird uebernommen." -f $app.AppId)
} else {
  $app = New-MgApplication -DisplayName $DisplayName -SignInAudience "AzureADMultipleOrgs"
  Write-Host ("App-Registrierung angelegt: '{0}' (AppId {1})" -f $app.DisplayName, $app.AppId) -ForegroundColor Green
}

# -- Delegierte Berechtigung Presence.Read ----------------------------------
# Der Scope-GUID wird dynamisch vom Graph-Service-Principal aufgelost, statt
# hart verdrahtet: die GUID ist ein Implementierungsdetail Microsofts und kann
# sich aendern, der menschenlesbare Scope-Name aber nicht.
$graphSpAppId = "00000003-0000-0000-c000-000000000000"
$sp = Get-MgServicePrincipal -Filter "appId eq '$graphSpAppId'"
# PowerShell-Zugriff auf PSCustomObject-Eigenschaften ist groessenunabhaengig,
# der Zugriff bleibt also robust gegen die Schreibweise des SDKs.
$scopeId = ($sp.Api.Oauth2PermissionScopes | Where-Object { $_.Value -eq "Presence.Read" } |
  Select-Object -First 1).Id
if (-not $scopeId) {
  Write-Error "Scope 'Presence.Read' im Graph-Service-Principal nicht gefunden -- kann nicht automatisch angelegt werden."
  exit 1
}

# requiredResourceAccess ist ein Replace-Feld: vorhandene Eintraege behalten,
# nur den Graph-Eintrag idempotent (neu) setzen.
$rra = @($app.RequiredResourceAccess | Where-Object { $_.ResourceAppId -ne $graphSpAppId })
$rra += [ordered]@{
  resourceAppId  = $graphSpAppId
  resourceAccess = @([ordered]@{ id = $scopeId; type = "Scope" })
}
Update-MgApplication -ApplicationId $app.Id -BodyParameter @{ requiredResourceAccess = $rra }
Write-Host "Delegierte Berechtigung 'Presence.Read' gesetzt." -ForegroundColor Green

Write-Host ""
Write-Host "Fertig. Client (Application) ID fuer die Companion-App:" -ForegroundColor Cyan
Write-Host ("  {0}" -f $app.AppId)
Write-Host ""
Write-Host "In die Companion-App unter Einstellungen -> 'Entra-Client-ID' eintragen."
