use keyring::Entry;
use reqwest::Client;
use serde::{Deserialize, Serialize};
use std::io::{Read, Write};
use std::net::TcpListener;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Mutex;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};

const GRAPH_SCOPE: &str = "https://graph.microsoft.com/Presence.Read offline_access openid profile";
const KEYRING_SERVICE: &str = "ruhr.ritter.pixelstatus.teams";

#[derive(Clone, Serialize, Deserialize)]
pub struct DeviceLogin {
    pub user_code: String,
    pub verification_uri: String,
    pub message: String,
    pub device_code: String,
    pub interval: u64,
}

#[derive(Clone, Serialize)]
pub struct TeamsAccount {
    pub display_name: String,
    pub username: String,
}

#[derive(Deserialize)]
struct DeviceResponse {
    device_code: String,
    user_code: String,
    verification_uri: Option<String>,
    verification_uri_complete: Option<String>,
    message: String,
    interval: Option<u64>,
}

#[derive(Deserialize)]
struct TokenResponse {
    access_token: Option<String>,
    refresh_token: Option<String>,
    id_token: Option<String>,
    expires_in: Option<u64>,
    error: Option<String>,
    error_description: Option<String>,
}

#[derive(Serialize, Deserialize)]
struct StoredToken {
    access_token: String,
    refresh_token: Option<String>,
    expires_at: u64,
    // Konto (UPN, sonst Anzeigename) direkt im Credential-Store: so ist der
    // Login-Status und der angezeigte Name unabhaengig von den App-Einstellungen,
    // die erst beim "Speichern" gemeldet werden. `default`, damit aeltere
    // Eintraege ohne das Feld sich trotzdem laden.
    #[serde(default)]
    account: Option<String>,
}

#[derive(Deserialize)]
struct GraphPresence {
    availability: Option<String>,
    activity: Option<String>,
}

fn tenant_base(tenant: &str) -> String {
    let tenant = if tenant.trim().is_empty() {
        "organizations"
    } else {
        tenant.trim()
    };
    format!("https://login.microsoftonline.com/{tenant}")
}

fn token_entry(client_id: &str) -> Result<Entry, String> {
    Entry::new(KEYRING_SERVICE, client_id).map_err(|e| format!("Token-Speicher: {e}"))
}

async fn token_request(
    client: &Client,
    endpoint: &str,
    params: &[(&str, &str)],
) -> Result<TokenResponse, String> {
    client
        .post(endpoint)
        .form(params)
        .send()
        .await
        .map_err(|e| format!("Microsoft-Anmeldung: {e}"))?
        .json::<TokenResponse>()
        .await
        .map_err(|e| format!("Microsoft-Antwort: {e}"))
}

pub async fn start_login(client_id: &str, tenant: &str) -> Result<DeviceLogin, String> {
    if client_id.trim().is_empty() {
        return Err("Für Teams muss eine Microsoft-Entra-Client-ID eingetragen werden.".into());
    }
    let client = Client::new();
    let endpoint = format!("{}/oauth2/v2.0/devicecode", tenant_base(tenant));
    let response = client
        .post(endpoint)
        .form(&[("client_id", client_id), ("scope", GRAPH_SCOPE)])
        .send()
        .await
        .map_err(|e| format!("Microsoft-Anmeldung: {e}"))?;
    if !response.status().is_success() {
        return Err(format!(
            "Microsoft-Anmeldung abgelehnt ({})",
            response.status()
        ));
    }
    let data = response
        .json::<DeviceResponse>()
        .await
        .map_err(|e| format!("Microsoft-Antwort: {e}"))?;
    Ok(DeviceLogin {
        user_code: data.user_code,
        verification_uri: data
            .verification_uri_complete
            .or(data.verification_uri)
            .unwrap_or_else(|| "https://microsoft.com/devicelogin".into()),
        message: data.message,
        device_code: data.device_code,
        interval: data.interval.unwrap_or(5).clamp(1, 15),
    })
}

pub async fn complete_login(
    client_id: &str,
    tenant: &str,
    login: DeviceLogin,
) -> Result<TeamsAccount, String> {
    let client = Client::new();
    let endpoint = format!("{}/oauth2/v2.0/token", tenant_base(tenant));
    let deadline = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs()
        + 900;
    loop {
        if SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs()
            >= deadline
        {
            return Err("Teams-Anmeldung ist abgelaufen.".into());
        }
        let response = token_request(
            &client,
            &endpoint,
            &[
                ("grant_type", "urn:ietf:params:oauth:grant-type:device_code"),
                ("client_id", client_id),
                ("device_code", &login.device_code),
            ],
        )
        .await?;
        if let Some(access_token) = response.access_token {
            return finish_login(
                client_id,
                access_token,
                response.refresh_token,
                response.id_token,
                response.expires_in,
            );
        }
        match response.error.as_deref() {
            Some("authorization_pending") => {}
            Some("slow_down") => tokio_sleep(login.interval + 5).await,
            Some(error) => {
                return Err(response.error_description.unwrap_or_else(|| error.into()));
            }
            None => return Err("Microsoft lieferte kein Zugriffstoken.".into()),
        }
        tokio_sleep(login.interval).await;
    }
}

// Gemeinsamer Abschluss beider Login-Flows: Token im nativen Credential-Store
// ablegen. Das Konto kommt aus den Token-Claims (ID-Token, sonst Access-Token):
// /me wuerde zusaetzlich User.Read brauchen, die App fordert aber bewusst nur
// Presence.Read an. Die Claims werden nur zur Anzeige gelesen, nicht geprueft.
fn finish_login(
    client_id: &str,
    access_token: String,
    refresh_token: Option<String>,
    id_token: Option<String>,
    expires_in: Option<u64>,
) -> Result<TeamsAccount, String> {
    let account = id_token
        .as_deref()
        .and_then(account_from_jwt)
        .or_else(|| account_from_jwt(&access_token))
        .unwrap_or(TeamsAccount {
            display_name: String::new(),
            username: String::new(),
        });
    let name = if account.username.is_empty() {
        account.display_name.clone()
    } else {
        account.username.clone()
    };
    let token = StoredToken {
        access_token,
        refresh_token,
        expires_at: now() + expires_in.unwrap_or(3600),
        account: if name.is_empty() { None } else { Some(name) },
    };
    token_entry(client_id)?
        .set_password(&serde_json::to_string(&token).map_err(|e| e.to_string())?)
        .map_err(|e| format!("Token-Speicher: {e}"))?;
    Ok(account)
}

// Liest Anzeigename und Benutzername (UPN/E-Mail) aus dem Payload eines JWT.
fn account_from_jwt(token: &str) -> Option<TeamsAccount> {
    use base64::Engine;
    let payload = token.split('.').nth(1)?;
    let bytes = base64::engine::general_purpose::URL_SAFE_NO_PAD
        .decode(payload.trim_end_matches('='))
        .ok()?;
    let claims: serde_json::Value = serde_json::from_slice(&bytes).ok()?;
    let claim = |keys: &[&str]| {
        keys.iter()
            .find_map(|k| {
                claims
                    .get(*k)
                    .and_then(|v| v.as_str())
                    .filter(|s| !s.is_empty())
            })
            .unwrap_or_default()
            .to_string()
    };
    let account = TeamsAccount {
        display_name: claim(&["name"]),
        username: claim(&["preferred_username", "upn", "email", "unique_name"]),
    };
    if account.display_name.is_empty() && account.username.is_empty() {
        None
    } else {
        Some(account)
    }
}

async fn tokio_sleep(seconds: u64) {
    tokio::time::sleep(Duration::from_secs(seconds)).await;
}

fn now() -> u64 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_secs()
}

async fn current_token(client_id: &str, tenant: &str) -> Result<StoredToken, String> {
    let raw = token_entry(client_id)?
        .get_password()
        .map_err(|_| "Nicht bei Teams angemeldet.".to_string())?;
    let mut token: StoredToken =
        serde_json::from_str(&raw).map_err(|e| format!("Token-Speicher: {e}"))?;
    if token.expires_at > now() + 60 {
        return Ok(token);
    }
    let refresh = token
        .refresh_token
        .clone()
        .ok_or("Teams-Anmeldung abgelaufen.".to_string())?;
    let client = Client::new();
    let response = token_request(
        &client,
        &format!("{}/oauth2/v2.0/token", tenant_base(tenant)),
        &[
            ("grant_type", "refresh_token"),
            ("client_id", client_id),
            ("refresh_token", &refresh),
            ("scope", GRAPH_SCOPE),
        ],
    )
    .await?;
    token.access_token = response
        .access_token
        .ok_or("Teams-Token konnte nicht erneuert werden.".to_string())?;
    if response.refresh_token.is_some() {
        token.refresh_token = response.refresh_token;
    }
    token.expires_at = now() + response.expires_in.unwrap_or(3600);
    token_entry(client_id)?
        .set_password(&serde_json::to_string(&token).map_err(|e| e.to_string())?)
        .map_err(|e| format!("Token-Speicher: {e}"))?;
    Ok(token)
}

pub async fn presence(client_id: &str, tenant: &str) -> Result<(String, String), String> {
    let client = Client::new();
    let token = current_token(client_id, tenant).await?;
    let response = client
        .get("https://graph.microsoft.com/v1.0/me/presence")
        .bearer_auth(token.access_token)
        .send()
        .await
        .map_err(|e| format!("Teams-Präsenz: {e}"))?;
    if !response.status().is_success() {
        return Err(format!(
            "Teams-Präsenz nicht verfügbar ({})",
            response.status()
        ));
    }
    let p = response
        .json::<GraphPresence>()
        .await
        .map_err(|e| e.to_string())?;
    Ok((
        p.availability.unwrap_or_default(),
        p.activity.unwrap_or_default(),
    ))
}

pub fn logout(client_id: &str) -> Result<(), String> {
    token_entry(client_id)?
        .delete_credential()
        .map_err(|e| format!("Teams-Abmeldung: {e}"))
}

// Login-Status + gespeicherter Konto-Name aus EINEM Keyring-Zugriff: None =
// nicht angemeldet (kein Token im nativen Credential-Store, unabhaengig davon,
// ob er gerade gueltig oder erneuerbar ist); Some(name) = angemeldet, name ggf.
// leer. Der Name wird beim Login neben dem Token abgelegt, damit er auch dann da
// ist, wenn die App-Einstellung (erst beim "Speichern" geschrieben) leer ist.
fn stored_login(client_id: &str) -> Option<String> {
    let raw = token_entry(client_id).ok()?.get_password().ok()?;
    let Ok(token) = serde_json::from_str::<StoredToken>(&raw) else {
        return Some(String::new());
    };
    // Aeltere Eintraege ohne Namen: aus den Claims des Access-Tokens ableiten,
    // damit kein erneuter Login noetig ist.
    Some(
        token
            .account
            .or_else(|| {
                account_from_jwt(&token.access_token).map(|a| {
                    if a.username.is_empty() {
                        a.display_name
                    } else {
                        a.username
                    }
                })
            })
            .unwrap_or_default(),
    )
}

#[derive(Serialize)]
pub struct TeamsPresence {
    pub logged_in: bool,
    // Konto-Name aus dem Token (Keyring); kann bei aelteren Eintraegen leer sein.
    pub account: String,
    pub availability: String,
    pub activity: String,
    // Fehlertext, falls die Praesenzabfrage (Trotz gueltigem Token) fehlschlug.
    pub error: Option<String>,
}

// Login-Status + aktuelle Teams-Praesenz; reine Abfrage ohne Nebenwirkungen.
// "angemeldet" ist der Keyring-Token -- NIE die App-Einstellung, die erst beim
// "Speichern" geschrieben wird und sonst einen Login verdeckt.
pub async fn status(client_id: &str, tenant: &str) -> TeamsPresence {
    let Some(account) = stored_login(client_id) else {
        return TeamsPresence::logged_out();
    };
    build_status(account, presence(client_id, tenant).await)
}

// Wie status(), aber mit bereits vorliegendem Praesenz-Ergebnis (z. B. dem
// zuletzt vom Auto-Status-Watcher abgefragten) -- keine eigene Graph-Abfrage.
pub fn status_with(client_id: &str, result: Result<(String, String), String>) -> TeamsPresence {
    match stored_login(client_id) {
        Some(account) => build_status(account, result),
        None => TeamsPresence::logged_out(),
    }
}

impl TeamsPresence {
    fn logged_out() -> Self {
        TeamsPresence {
            logged_in: false,
            account: String::new(),
            availability: String::new(),
            activity: String::new(),
            error: None,
        }
    }
}

fn build_status(account: String, result: Result<(String, String), String>) -> TeamsPresence {
    match result {
        Ok((availability, activity)) => TeamsPresence {
            logged_in: true,
            account,
            availability,
            activity,
            error: None,
        },
        Err(e) => TeamsPresence {
            logged_in: true,
            account,
            availability: String::new(),
            activity: String::new(),
            error: Some(e),
        },
    }
}

// ---------------------------------------------------------------------------
// Browser-Login (Authorization-Code-Flow mit PKCE + Loopback-Redirect)
//
// Alternative zum Device-Code-Flow, der in vielen Tenants gesperrt ist. Die
// App hoert kurz auf dem festen Loopback-Port 127.0.0.1:8939 auf (dieser muss
// in der App-Registrierung stehen -- ein dynamischer/Wildcard-Port wird von
// manchen Tenants mit AADSTS50011 abgelehnt), oeffnet den Browser auf der
// Anmelde-URL und tauscht den zurueckgelieferten Code gegen Tokens ein.
// PKCE (S256) schuetzt den Code-Exchange; ein Client-Secret gibt es bewusst
// nicht (Public Client).
// ---------------------------------------------------------------------------

#[derive(Serialize)]
pub struct BrowserLoginStart {
    pub authorize_url: String,
}

struct BrowserLoginState {
    listener: TcpListener,
    code_verifier: String,
    state: String,
    client_id: String,
    tenant: String,
    redirect_uri: String,
    generation: u64,
}

// Ein Eintrag pro Client: browser_start_login legt ihn an, browser_complete_login
// konsumiert ihn. Datei-statischer Mutex (Analogon zum MqttBridge-s_self), da es
// je Client-ID nur einen laufenden Login gibt.
static BROWSER_LOGIN: Mutex<Option<BrowserLoginState>> = Mutex::new(None);
// Zaehlt Login-Starts hoch; ein wartender Worker bricht ab, sobald seine
// Generation nicht mehr die aktuelle ist.
static BROWSER_LOGIN_GEN: AtomicU64 = AtomicU64::new(0);
const CALLBACK_ADDR: &str = "127.0.0.1:8939";

const BROWSER_LOGIN_TIMEOUT_SECS: u64 = 300;

fn urlenc(s: &str) -> String {
    let mut out = String::new();
    for b in s.bytes() {
        match b {
            b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'-' | b'_' | b'.' | b'~' => {
                out.push(b as char)
            }
            _ => out.push_str(&format!("%{:02X}", b)),
        }
    }
    out
}

fn urldecode(s: &str) -> String {
    let bytes = s.as_bytes();
    let mut out = Vec::with_capacity(bytes.len());
    let mut i = 0;
    while i < bytes.len() {
        match bytes[i] {
            b'%' if i + 2 < bytes.len() => {
                let hex = &s[i + 1..i + 3];
                if let Ok(n) = u8::from_str_radix(hex, 16) {
                    out.push(n);
                    i += 3;
                } else {
                    out.push(bytes[i]);
                    i += 1;
                }
            }
            b'+' => {
                out.push(b' ');
                i += 1;
            }
            c => {
                out.push(c);
                i += 1;
            }
        }
    }
    String::from_utf8_lossy(&out).into_owned()
}

// "GET /callback?code=…&state=… HTTP/1.1" -> der Querystring (nach "?", vor " HTTP/…").
fn extract_query(first_line: &str) -> &str {
    first_line
        .split_once('?')
        .and_then(|(_, q)| q.split_once(' '))
        .map(|(q, _)| q)
        .unwrap_or("")
}

fn parse_query_params(query: &str) -> std::collections::HashMap<String, String> {
    let mut params = std::collections::HashMap::new();
    for pair in query.split('&') {
        if let Some((k, v)) = pair.split_once('=') {
            params.insert(urldecode(k), urldecode(v));
        }
    }
    params
}

// PKCE: Verifier (zufaellig) + Challenge (BASE64URL(SHA256(Verifier))).
fn pkce() -> (String, String) {
    use base64::Engine;
    use rand::RngCore;
    use sha2::Digest;
    let enc = base64::engine::general_purpose::URL_SAFE_NO_PAD;
    let mut bytes = [0u8; 64];
    rand::thread_rng().fill_bytes(&mut bytes);
    let verifier = enc.encode(bytes);
    let challenge = enc.encode(sha2::Sha256::digest(verifier.as_bytes()));
    (verifier, challenge)
}

fn random_state() -> String {
    use base64::Engine;
    use rand::RngCore;
    let mut bytes = [0u8; 32];
    rand::thread_rng().fill_bytes(&mut bytes);
    base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(bytes)
}

/// Bindet den Loopback-Listener, erzeugt PKCE/State und liefert die
/// Anmelde-URL. Das eigentliche Warten/Erteilen laeuft in browser_complete_login.
pub fn browser_start_login(client_id: &str, tenant: &str) -> Result<BrowserLoginStart, String> {
    if client_id.trim().is_empty() {
        return Err("Für Teams muss eine Microsoft-Entra-Client-ID eingetragen werden.".into());
    }
    // Bereits laufenden Login beenden und dessen Listener (Port 8939) freigeben,
    // sonst scheitert der Bind unten mit "address already in use" (z. B. Doppelklick
    // auf "Anmelden"). Ein noch nicht abgeholter Login wird direkt verworfen; ein
    // bereits wartender (Listener liegt dann im Worker-Thread) wird ueber die
    // Generation abgebrochen und gibt den Port innerhalb eines Poll-Intervalls frei.
    *BROWSER_LOGIN.lock().unwrap() = None;
    let generation = BROWSER_LOGIN_GEN.fetch_add(1, Ordering::SeqCst) + 1;
    // Fester Loopback-Port statt dynamisch: die Redirect-URI muss exakt in der
    // App-Registrierung stehen; der Wildcard-Trick (localhost/127.0.0.1:0) wird
    // von manchen Tenants/Plattformen nicht erkannt (AADSTS50011). 127.0.0.1 statt
    // localhost vermeidet, dass der Browser localhost auf ::1 (IPv6) loest, waehrend
    // wir nur 127.0.0.1 (IPv4) binden. Ein fremder Prozess auf dem Port koennte den
    // Auth-Code zwar abgreifen, ohne den PKCE-Verifier aber nicht einloesen.
    let listener = bind_callback_listener()?;
    let redirect_uri = format!("http://{CALLBACK_ADDR}/callback");
    let (code_verifier, code_challenge) = pkce();
    let state = random_state();
    let authorize_url = format!(
        "{}/oauth2/v2.0/authorize?client_id={}&response_type=code&redirect_uri={}&scope={}&state={}&code_challenge={}&code_challenge_method=S256",
        tenant_base(tenant),
        urlenc(client_id),
        urlenc(&redirect_uri),
        urlenc(GRAPH_SCOPE),
        urlenc(&state),
        urlenc(&code_challenge),
    );
    *BROWSER_LOGIN.lock().unwrap() = Some(BrowserLoginState {
        listener,
        code_verifier,
        state,
        client_id: client_id.to_string(),
        tenant: tenant.to_string(),
        redirect_uri,
        generation,
    });
    Ok(BrowserLoginStart { authorize_url })
}

// Bindet den festen Callback-Port. Ein abgebrochener Vorgang gibt ihn erst beim
// naechsten Poll seines Worker-Threads frei, daher kurz erneut versuchen.
fn bind_callback_listener() -> Result<TcpListener, String> {
    let deadline = Instant::now() + Duration::from_millis(1000);
    loop {
        match TcpListener::bind(CALLBACK_ADDR) {
            Ok(l) => return Ok(l),
            Err(e) if e.kind() == std::io::ErrorKind::AddrInUse && Instant::now() < deadline => {
                std::thread::sleep(Duration::from_millis(50));
            }
            Err(e) => return Err(format!("Callback-Server {CALLBACK_ADDR}: {e}")),
        }
    }
}

// Minimales HTML-Escaping fuer Text, der in die Callback-Seite eingesetzt wird.
fn html_escape(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for c in s.chars() {
        match c {
            '&' => out.push_str("&amp;"),
            '<' => out.push_str("&lt;"),
            '>' => out.push_str("&gt;"),
            '"' => out.push_str("&quot;"),
            '\'' => out.push_str("&#39;"),
            _ => out.push(c),
        }
    }
    out
}

// Blockiert (in einem Worker-Thread) auf den einen Callback-Request, prueft den
// State (CSRF), antwortet dem Browser und liefert den Auth-Code zurueck.
fn receive_auth_code(
    listener: TcpListener,
    expected_state: &str,
    generation: u64,
) -> Result<String, String> {
    listener
        .set_nonblocking(true)
        .map_err(|e| format!("Callback-Server: {e}"))?;
    let deadline = Instant::now() + Duration::from_secs(BROWSER_LOGIN_TIMEOUT_SECS);
    let mut stream = loop {
        match listener.accept() {
            Ok((s, _)) => break s,
            Err(ref e) if e.kind() == std::io::ErrorKind::WouldBlock => {
                if BROWSER_LOGIN_GEN.load(Ordering::SeqCst) != generation {
                    return Err("Anmeldung durch einen neuen Vorgang abgebrochen.".into());
                }
                if Instant::now() >= deadline {
                    return Err("Zeitüberschreitung: keine Browser-Anmeldung erhalten.".into());
                }
                std::thread::sleep(Duration::from_millis(150));
            }
            Err(e) => return Err(format!("Callback-Server: {e}")),
        }
    };
    let _ = stream.set_nonblocking(false);

    let mut buf: Vec<u8> = Vec::new();
    let mut tmp = [0u8; 1024];
    loop {
        let n = stream
            .read(&mut tmp)
            .map_err(|e| format!("Callback-Lesefehler: {e}"))?;
        if n == 0 {
            break;
        }
        buf.extend_from_slice(&tmp[..n]);
        if buf.windows(2).any(|w| w == b"\r\n") || buf.len() > 8192 {
            break;
        }
    }
    let req = String::from_utf8_lossy(&buf);
    let first_line = req.lines().next().unwrap_or("");
    let params = parse_query_params(extract_query(first_line));

    let state = params.get("state").cloned().unwrap_or_default();
    let outcome: Result<String, String> = if state != expected_state {
        Err("Der State passt nicht -- Anmeldung abgebrochen (CSRF).".into())
    } else if let Some(err) = params.get("error").cloned() {
        let desc = params.get("error_description").cloned().unwrap_or_default();
        Err(format!("Microsoft: {err} {desc}"))
    } else if let Some(c) = params.get("code").cloned() {
        Ok(c)
    } else {
        Err("Kein Auth-Code in der Antwort erhalten.".into())
    };

    // Dem Browser eine kurze Rueckmeldung geben (Erfolg/Fehler) und ihn schliessen.
    let (title, body) = match &outcome {
        Ok(_) => (
            "Erfolg",
            "Die Anmeldung war erfolgreich. Dieses Fenster kann geschlossen werden.",
        ),
        Err(e) => ("Abgelehnt", e.as_str()),
    };
    let html = format!(
        "<!doctype html><html><head><meta charset=\"utf-8\"><title>PixelStatus</title></head>\
         <body style=\"font-family:system-ui,sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;margin:0;background:#0b0f14;color:#e6edf3\">\
         <div style=\"text-align:center\"><h1 style=\"margin:0 0 8px\">{t}</h1><p>{b}</p></div></body></html>",
        t = title,
        b = html_escape(body)
    );
    let resp = format!(
        "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{}",
        html.len(),
        html
    );
    let _ = stream.write_all(resp.as_bytes());

    outcome
}

/// Wartet auf den Browser-Callback, tauscht den Code (PKCE) gegen Tokens ein und
/// legt diese ab. Liefert (Konto, Client-ID, Tenant) zurueck.
pub async fn browser_complete_login() -> Result<(TeamsAccount, String, String), String> {
    let bl = BROWSER_LOGIN
        .lock()
        .unwrap()
        .take()
        .ok_or_else(|| "Kein laufender Browser-Login.".to_string())?;
    let expected_state = bl.state;
    let code_verifier = bl.code_verifier;
    let client_id = bl.client_id;
    let tenant = bl.tenant;
    let redirect_uri = bl.redirect_uri;
    let listener = bl.listener;
    let generation = bl.generation;

    let join = tokio::task::spawn_blocking(move || {
        receive_auth_code(listener, &expected_state, generation)
    })
    .await;
    let code_result = join.map_err(|e| format!("Browser-Login-Thread fehlgeschlagen: {e}"))?;
    let code = code_result?;

    let client = Client::new();
    let endpoint = format!("{}/oauth2/v2.0/token", tenant_base(&tenant));
    let response = token_request(
        &client,
        &endpoint,
        &[
            ("grant_type", "authorization_code"),
            ("client_id", &client_id),
            ("code", &code),
            ("redirect_uri", &redirect_uri),
            ("code_verifier", &code_verifier),
            ("scope", GRAPH_SCOPE),
        ],
    )
    .await?;
    let access_token = response.access_token.ok_or_else(|| {
        response
            .error_description
            .unwrap_or_else(|| "Kein Zugriffstoken erhalten.".into())
    })?;
    let account = finish_login(
        &client_id,
        access_token,
        response.refresh_token,
        response.id_token,
        response.expires_in,
    )?;
    Ok((account, client_id, tenant))
}

#[cfg(test)]
mod tests {
    use super::*;
    use sha2::Digest;

    #[test]
    fn account_from_jwt_reads_claims() {
        use base64::Engine;
        let enc = base64::engine::general_purpose::URL_SAFE_NO_PAD;
        let payload = enc.encode(r#"{"name":"Ada L","preferred_username":"ada@example.com"}"#);
        let a = account_from_jwt(&format!("h.{payload}.s")).unwrap();
        assert_eq!(a.display_name, "Ada L");
        assert_eq!(a.username, "ada@example.com");
        let upn_only = enc.encode(r#"{"upn":"bob@example.com"}"#);
        assert_eq!(
            account_from_jwt(&format!("h.{upn_only}.s"))
                .unwrap()
                .username,
            "bob@example.com"
        );
        assert!(account_from_jwt("not-a-jwt").is_none());
    }

    #[test]
    fn urlenc_roundtrip() {
        let s = "https://graph.microsoft.com/Presence.Read offline_access openid profile";
        let enc = urlenc(s);
        assert!(!enc.contains(' '));
        assert_eq!(urldecode(&enc), s);
    }

    #[test]
    fn urlenc_keeps_unreserved() {
        assert_eq!(urlenc("abcXYZ019-_.~"), "abcXYZ019-_.~");
        assert_eq!(urlenc("a b"), "a%20b");
    }

    #[test]
    fn extract_query_strips_path_and_version() {
        let line = "GET /callback?code=AbC123&state=xyz_-5 HTTP/1.1";
        assert_eq!(extract_query(line), "code=AbC123&state=xyz_-5");
    }

    #[test]
    fn parse_query_params_decodes() {
        let p = parse_query_params("code=AbC123&state=a%20b&error=access_denied");
        assert_eq!(p.get("code").map(String::as_str), Some("AbC123"));
        assert_eq!(p.get("state").map(String::as_str), Some("a b"));
        assert_eq!(p.get("error").map(String::as_str), Some("access_denied"));
    }

    #[test]
    fn pkce_challenge_matches_s256() {
        use base64::Engine;
        let (verifier, challenge) = pkce();
        let expected = base64::engine::general_purpose::URL_SAFE_NO_PAD
            .encode(sha2::Sha256::digest(verifier.as_bytes()));
        assert_eq!(challenge, expected);
        assert!(verifier.len() >= 43 && verifier.len() <= 128);
    }
}
