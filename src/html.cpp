#include "main.h"
#include "config_backup.h"

namespace
{
constexpr char kFirmwareUpdatePath[] = "/firmware";
constexpr char kConfigExportPath[] = "/config/export";
constexpr char kConfigImportPath[] = "/config/import";
constexpr char kDiagnosticsPath[] = "/diagnostics";
constexpr char kConsolePath[] = "/console";
constexpr char kConsoleLogPath[] = "/console/log";
constexpr char kConsoleCommandPath[] = "/console/command";
constexpr char kConsoleDownloadPath[] = "/console/download";
constexpr char kConsoleClearPath[] = "/console/clear";
constexpr char kDisplayTestPath[] = "/display-test";
constexpr char kGpsActionPath[] = "/gps/action";
constexpr char kGpsRawPath[] = "/gps/raw";
constexpr char kThemeCssPath[] = "/theme.css";
constexpr char kLoginPath[] = "/login";
constexpr char kLogoutPath[] = "/logout";
constexpr char kOnboardingPath[] = "/onboarding";
constexpr char kOnboardingMatrixTestPath[] = "/onboarding/self-test/matrix";
constexpr size_t kMaxConfigImportBytes = 24U * 1024U;
constexpr uint32_t kLiveRefreshIntervalMs = 15000UL;
constexpr uint32_t kConsoleAccessTokenLifetimeMs = 30UL * 60UL * 1000UL;
constexpr uint32_t kWebSessionLifetimeSeconds = 7UL * 24UL * 60UL * 60UL;
constexpr char kWebSessionCookieName[] = "ledclock_session";
const char *kCollectedHeaderKeys[] = {"Cookie"};

/** Tracks whether the current config-import upload request passed auth checks. */
bool configImportAuthenticated = false;
/** Tracks whether a config backup upload has started successfully. */
bool configImportStarted = false;
/** Holds the fatal error message for the current config-import request. */
String configImportError;
/** Accumulates the uploaded JSON backup body before it is parsed and applied. */
String configImportPayload;

/** Tracks whether the current firmware upload request passed auth checks. */
bool firmwareUploadAuthenticated = false;
/** Tracks whether a firmware upload has started successfully. */
bool firmwareUploadStarted = false;
/** Holds the fatal error message for the current firmware-upload request. */
String firmwareUploadError;
/** Tracks whether the uploaded OTA image looks like an ESP application binary. */
bool firmwareUploadLooksLikeAppImage = false;
/** Temporary console-access token embedded into the live console page for follow-up fetches. */
char consoleAccessToken[17] = "";
/** Millisecond timestamp when the current console-access token was issued. */
uint32_t consoleAccessTokenIssuedAt = 0;
/** Session token used by the custom password login for the normal web UI. */
char webSessionToken[33] = "";
/** Millisecond timestamp when the current web login session token was issued. */
uint32_t webSessionIssuedAt = 0;

bool authorizeAdminRequest();

/** Captures the current onboarding form values before they are persisted. */
struct OnboardingDraft
{
  String wifiSsid;
  String wifiPassword;
  String adminPassword;
  String weatherApiKey;
  String ipGeoApiKey;
  bool enableWebPassword = true;
  bool useFixedTimezone = false;
  int8_t fixedOffset = 0;
};

const char kWebThemeCss[] PROGMEM = R"clockcss(
:root{
  --bg-top:#f7f1e7;
  --bg-bottom:#e6ddd2;
  --surface:rgba(255,255,255,0.86);
  --surface-strong:rgba(255,255,255,0.95);
  --line:rgba(27,36,48,0.12);
  --text:#1d2935;
  --muted:#5e6a72;
  --hero:#173748;
  --hero-2:#245766;
  --accent:#0f766e;
  --accent-soft:#d8f1ed;
  --warm:#c7671e;
  --warm-soft:#f7e7d8;
  --good:#166534;
  --good-soft:#ddf4e3;
  --bad:#b45309;
  --bad-soft:#fee9dc;
  --shadow:0 18px 45px rgba(22,44,58,0.12);
  --shell-bg:#e6ddd2;
  --hero-lede:rgba(255,255,255,0.84);
  --hero-chip-bg:rgba(255,255,255,0.14);
  --hero-chip-border:rgba(255,255,255,0.16);
  --soft-chip-bg:rgba(255,255,255,0.72);
  --soft-chip-border:rgba(255,255,255,0.5);
  --soft-chip-text:var(--hero);
  --tile-bg:rgba(255,255,255,0.9);
  --tile-border:rgba(23,55,72,0.08);
  --tile-shadow:0 8px 22px rgba(22,44,58,0.05);
  --panel-bg:linear-gradient(180deg, rgba(255,255,255,0.96), rgba(245,239,231,0.92));
  --section-bg:linear-gradient(180deg, rgba(216,241,237,0.26), rgba(255,255,255,0.96));
  --field-bg:rgba(255,255,255,0.94);
  --field-border:rgba(15,118,110,0.2);
  --field-shadow:inset 0 1px 0 rgba(255,255,255,0.6);
  --secondary-button-bg:rgba(255,255,255,0.14);
  --secondary-button-border:rgba(255,255,255,0.16);
  --secondary-button-text:#f3f7f8;
  --ghost-button-bg:rgba(255,255,255,0.84);
  --ghost-button-border:rgba(23,55,72,0.12);
  --ghost-button-text:var(--hero);
  --console-bg:#11222d;
  --console-text:#dbe9ee;
}
html[data-web-theme='dark']{
  --bg-top:#08131b;
  --bg-bottom:#101d28;
  --surface:rgba(14,23,31,0.9);
  --surface-strong:rgba(18,29,38,0.96);
  --line:rgba(148,177,194,0.18);
  --text:#e8f0f4;
  --muted:#a6bac6;
  --hero:#08151e;
  --hero-2:#123141;
  --accent:#36b2a4;
  --accent-soft:rgba(54,178,164,0.16);
  --warm:#ef9a56;
  --warm-soft:rgba(239,154,86,0.16);
  --good:#4fc37a;
  --good-soft:rgba(79,195,122,0.16);
  --bad:#f08b63;
  --bad-soft:rgba(240,139,99,0.16);
  --shadow:0 18px 45px rgba(0,0,0,0.35);
  --shell-bg:#101d28;
  --hero-lede:rgba(232,240,244,0.82);
  --hero-chip-bg:rgba(22,35,45,0.74);
  --hero-chip-border:rgba(167,196,211,0.14);
  --soft-chip-bg:rgba(22,35,45,0.78);
  --soft-chip-border:rgba(167,196,211,0.14);
  --soft-chip-text:#d9e9f1;
  --tile-bg:rgba(19,31,40,0.92);
  --tile-border:rgba(167,196,211,0.12);
  --tile-shadow:0 8px 22px rgba(0,0,0,0.18);
  --panel-bg:linear-gradient(180deg, rgba(20,33,43,0.98), rgba(14,23,31,0.92));
  --section-bg:linear-gradient(180deg, rgba(54,178,164,0.14), rgba(19,31,40,0.96));
  --field-bg:rgba(10,18,24,0.9);
  --field-border:rgba(54,178,164,0.28);
  --field-shadow:inset 0 1px 0 rgba(255,255,255,0.03);
  --secondary-button-bg:rgba(22,35,45,0.74);
  --secondary-button-border:rgba(167,196,211,0.14);
  --secondary-button-text:#e8f0f4;
  --ghost-button-bg:rgba(22,35,45,0.92);
  --ghost-button-border:rgba(167,196,211,0.14);
  --ghost-button-text:#e8f0f4;
  --console-bg:#071117;
  --console-text:#dbe9ee;
}
*{box-sizing:border-box}
html{background:var(--shell-bg)}
body{
  margin:0;
  min-height:100vh;
  color:var(--text);
  font-family:"Trebuchet MS","Gill Sans","Segoe UI",sans-serif;
  background:
    radial-gradient(circle at top left, rgba(15,118,110,0.16), transparent 30%),
    radial-gradient(circle at top right, rgba(199,103,30,0.18), transparent 34%),
    linear-gradient(180deg, var(--bg-top) 0%, var(--bg-bottom) 100%);
}
a{color:var(--accent);text-decoration:none}
a:hover{text-decoration:underline}
.shell,.portal-shell{max-width:1320px;margin:0 auto;padding:28px 18px 48px}
.hero,.portal-hero{
  position:relative;
  overflow:hidden;
  padding:28px;
  border-radius:28px;
  color:#fff;
  background:linear-gradient(135deg, var(--hero), var(--hero-2));
  box-shadow:var(--shadow);
}
.hero:before,.portal-hero:before{
  content:"";
  position:absolute;
  inset:auto -70px -85px auto;
  width:240px;
  height:240px;
  background:radial-gradient(circle, rgba(255,255,255,0.24), transparent 66%);
  pointer-events:none;
}
.eyebrow,.portal-eyebrow{
  margin:0;
  opacity:0.82;
  font-size:0.72rem;
  font-weight:700;
  letter-spacing:0.16em;
  text-transform:uppercase;
}
.hero h1,.portal-hero h1{
  margin:10px 0 8px;
  font-family:"Georgia","Times New Roman",serif;
  font-size:clamp(2rem,4vw,3.4rem);
  line-height:1.06;
}
.lede,.portal-lede{
  max-width:52rem;
  margin:0;
  color:var(--hero-lede);
  line-height:1.6;
}
.action-row,.portal-chip-row,.portal-nav{
  display:flex;
  flex-wrap:wrap;
  gap:10px;
  margin-top:18px;
}
.button-link,.portal-inline-action{
  display:inline-flex;
  align-items:center;
  justify-content:center;
  gap:8px;
  padding:11px 16px;
  border-radius:999px;
  font-weight:700;
  border:0;
  text-decoration:none;
  transition:transform 120ms ease, box-shadow 120ms ease;
}
.button-link:hover,.portal-inline-action:hover{transform:translateY(-1px);text-decoration:none}
.button-link.primary,.portal-inline-action{
  color:#fff;
  background:linear-gradient(135deg, var(--accent), #149a90);
  box-shadow:0 10px 24px rgba(15,118,110,0.24);
}
.button-link.secondary{
  color:var(--secondary-button-text);
  background:var(--secondary-button-bg);
  border:1px solid var(--secondary-button-border);
}
.button-link.ghost,.portal-inline-action.ghost{
  color:var(--ghost-button-text);
  background:var(--ghost-button-bg);
  border:1px solid var(--ghost-button-border);
  box-shadow:0 10px 24px rgba(22,44,58,0.08);
}
.button-link.danger,.portal-inline-action.danger{
  color:#fff;
  background:linear-gradient(135deg, #b84c2a, #d97706);
  box-shadow:0 10px 24px rgba(184,76,42,0.24);
}
.status-chip{
  display:inline-flex;
  align-items:center;
  flex-wrap:wrap;
  gap:8px;
  padding:10px 14px;
  border-radius:999px;
  font-size:0.9rem;
  font-weight:700;
  background:var(--hero-chip-bg);
  border:1px solid var(--hero-chip-border);
}
.status-chip strong{font-size:0.78rem;opacity:0.72;text-transform:uppercase;letter-spacing:0.08em}
.metric-grid{
  display:grid;
  grid-template-columns:repeat(auto-fit, minmax(180px, 1fr));
  gap:14px;
  margin-top:22px;
}
.metric-card,.setup-step,.portal-card,.card{
  border-radius:24px;
  background:var(--surface);
  border:1px solid var(--hero-chip-border);
  box-shadow:0 14px 34px rgba(22,44,58,0.08);
  backdrop-filter:blur(10px);
}
.metric-card{
  padding:18px;
  color:var(--text);
}
.metric-card span{
  display:block;
  margin-bottom:8px;
  color:var(--muted);
  font-size:0.82rem;
  font-weight:700;
  letter-spacing:0.08em;
  text-transform:uppercase;
}
.metric-card strong{
  display:block;
  font-family:"Georgia","Times New Roman",serif;
  font-size:1.45rem;
  line-height:1.2;
  overflow-wrap:anywhere;
  word-break:break-word;
}
.card-span{grid-column:1 / -1}
.service-health-grid{
  display:grid;
  grid-template-columns:repeat(auto-fit, minmax(220px, 1fr));
  gap:14px;
  margin-top:18px;
}
.service-health-card{
  padding:16px 18px;
  border-radius:20px;
  border:1px solid var(--tile-border);
  box-shadow:0 10px 24px rgba(22,44,58,0.06);
}
.service-health-card h3{
  margin:0;
  font-family:"Georgia","Times New Roman",serif;
  font-size:1.1rem;
}
.service-health-card-head{
  display:flex;
  align-items:flex-start;
  justify-content:space-between;
  gap:12px;
}
.service-health-card-head span{
  display:inline-flex;
  align-items:center;
  justify-content:center;
  min-height:32px;
  padding:6px 10px;
  border-radius:999px;
  font-size:0.75rem;
  font-weight:700;
  letter-spacing:0.08em;
  text-transform:uppercase;
  color:var(--soft-chip-text);
  background:var(--soft-chip-bg);
  border:1px solid var(--soft-chip-border);
}
.service-health-card p{
  margin:12px 0 0;
  color:var(--muted);
  line-height:1.5;
}
.service-health-meta{
  display:flex;
  align-items:center;
  justify-content:space-between;
  gap:12px;
  margin-top:14px;
  padding-top:12px;
  border-top:1px solid var(--line);
}
.service-health-meta span{
  color:var(--muted);
  font-size:0.76rem;
  font-weight:700;
  letter-spacing:0.08em;
  text-transform:uppercase;
}
.service-health-meta strong{
  font-size:0.95rem;
}
.tone-good{background:linear-gradient(180deg, var(--good-soft), var(--surface-strong))}
.tone-warn{background:linear-gradient(180deg, var(--warm-soft), var(--surface-strong))}
.tone-bad{background:linear-gradient(180deg, var(--bad-soft), var(--surface-strong))}
.tone-neutral{background:linear-gradient(180deg, var(--accent-soft), var(--surface-strong))}
.notice{
  margin-top:18px;
  padding:16px 18px;
  border-radius:20px;
  border:1px solid var(--line);
  font-weight:700;
  box-shadow:0 10px 22px rgba(22,44,58,0.06);
}
.notice-good{background:var(--good-soft)}
.notice-warn{background:var(--warm-soft)}
.notice-bad{background:var(--bad-soft)}
.content-grid{
  display:grid;
  grid-template-columns:repeat(auto-fit, minmax(340px, 1fr));
  gap:18px;
  margin-top:20px;
}
.card,.portal-card{padding:22px}
.card-header h2{
  margin:0 0 6px;
  font-family:"Georgia","Times New Roman",serif;
  font-size:1.45rem;
}
.card-subtitle{
  margin:0;
  color:var(--muted);
  line-height:1.55;
}
.kv-list{margin-top:16px}
.kv-row{
  display:grid;
  grid-template-columns:minmax(120px, 176px) minmax(0, 1fr);
  gap:10px;
  padding:12px 0;
  border-top:1px solid rgba(27,36,48,0.08);
  align-items:start;
}
.kv-row:first-child{border-top:0;padding-top:0}
.kv-row dt{
  margin:0;
  color:var(--muted);
  font-size:0.82rem;
  font-weight:700;
  letter-spacing:0.08em;
  text-transform:uppercase;
}
.kv-row dd{
  margin:0;
  text-align:left;
  font-weight:700;
  min-width:0;
  max-width:100%;
  overflow-wrap:anywhere;
  word-break:break-word;
}
.dashboard-clamp{
  display:-webkit-box;
  max-width:100%;
  -webkit-box-orient:vertical;
  -webkit-line-clamp:2;
  overflow:hidden;
  text-overflow:ellipsis;
  overflow-wrap:anywhere;
  word-break:break-word;
  line-height:1.35;
  vertical-align:top;
}
.setup-steps{
  display:grid;
  grid-template-columns:repeat(auto-fit, minmax(220px, 1fr));
  gap:16px;
  margin-top:20px;
}
.setup-step{
  padding:20px;
  background:var(--panel-bg);
}
.setup-step h3{
  margin:0 0 8px;
  font-family:"Georgia","Times New Roman",serif;
  font-size:1.15rem;
}
.setup-step p{
  margin:0;
  color:var(--muted);
  line-height:1.55;
}
.auth-shell{
  max-width:880px;
  margin:0 auto;
  padding:28px 18px 48px;
}
.auth-card,.wizard-card{
  padding:22px;
  border-radius:24px;
  background:var(--surface);
  border:1px solid var(--hero-chip-border);
  box-shadow:0 14px 34px rgba(22,44,58,0.08);
  backdrop-filter:blur(10px);
}
.auth-form,.onboarding-form{
  display:grid;
  gap:18px;
  margin-top:18px;
}
.field-grid{
  display:grid;
  grid-template-columns:repeat(auto-fit, minmax(240px, 1fr));
  gap:14px;
}
.form-field{
  display:grid;
  gap:8px;
}
.form-field label{
  display:block;
  color:var(--muted);
  font-size:0.78rem;
  font-weight:700;
  letter-spacing:0.08em;
  text-transform:uppercase;
}
.form-field input,.form-field select{
  width:100%;
  border:1px solid var(--field-border);
  border-radius:14px;
  background:var(--field-bg);
  color:var(--text);
  font:inherit;
  padding:12px 14px;
  box-shadow:var(--field-shadow);
}
.form-field input:focus,.form-field select:focus{
  outline:2px solid rgba(15,118,110,0.18);
  border-color:var(--accent);
}
.field-help,.auth-note{
  margin:0;
  color:var(--muted);
  line-height:1.55;
}
.field-help strong,.auth-note strong{
  color:var(--hero);
}
.wizard-progress{
  display:flex;
  flex-wrap:wrap;
  gap:10px;
  margin-top:18px;
}
.step-pill{
  display:inline-flex;
  align-items:center;
  gap:8px;
  padding:10px 14px;
  border-radius:999px;
  background:var(--soft-chip-bg);
  border:1px solid var(--soft-chip-border);
  color:var(--soft-chip-text);
  font-size:0.85rem;
  font-weight:700;
  box-shadow:0 8px 18px rgba(22,44,58,0.08);
}
.step-pill strong{
  display:inline-flex;
  align-items:center;
  justify-content:center;
  width:1.4rem;
  height:1.4rem;
  border-radius:999px;
  background:rgba(23,55,72,0.08);
  font-size:0.74rem;
}
.onboarding-step{
  margin-top:18px;
}
.onboarding-step[hidden]{
  display:none;
}
.onboarding-actions{
  display:flex;
  flex-wrap:wrap;
  gap:12px;
  align-items:center;
  justify-content:space-between;
}
.onboarding-actions .spacer{
  flex:1 1 auto;
}
.self-test-grid{
  display:grid;
  grid-template-columns:repeat(auto-fit, minmax(220px, 1fr));
  gap:14px;
  margin-top:18px;
}
.self-test-item{
  padding:16px;
  border-radius:18px;
  border:1px solid var(--tile-border);
  background:var(--tile-bg);
  box-shadow:var(--tile-shadow);
}
.self-test-item h3{
  margin:0 0 6px;
  font-family:"Georgia","Times New Roman",serif;
  font-size:1.05rem;
  color:var(--hero);
}
.self-test-item p{
  margin:0;
  color:var(--muted);
  line-height:1.55;
}
.recovery-panel{
  margin-top:18px;
  padding:18px;
  border-radius:20px;
  background:linear-gradient(180deg, var(--warm-soft), var(--surface-strong));
  border:1px solid rgba(199,103,30,0.18);
}
.recovery-panel h2{
  margin:0 0 8px;
  font-family:"Georgia","Times New Roman",serif;
  font-size:1.2rem;
  color:var(--hero);
}
.recovery-panel p{
  margin:0;
  color:var(--muted);
  line-height:1.6;
}
.portal-nav a{
  padding:10px 14px;
  border-radius:999px;
  background:var(--soft-chip-bg);
  border:1px solid var(--soft-chip-border);
  color:var(--soft-chip-text);
  font-weight:700;
  box-shadow:0 8px 18px rgba(22,44,58,0.08);
}
.portal-mode-bar{
  display:flex;
  flex-wrap:wrap;
  align-items:center;
  justify-content:space-between;
  gap:12px;
  margin-top:16px;
}
.portal-mode-help{
  margin:0;
  color:var(--muted);
  line-height:1.55;
}
.portal-mode-toggle{
  display:flex;
  flex-wrap:wrap;
  gap:10px;
}
.portal-mode-toggle button{
  width:auto;
  background:var(--soft-chip-bg);
  color:var(--soft-chip-text);
  border:1px solid var(--soft-chip-border);
  box-shadow:0 10px 24px rgba(22,44,58,0.08);
}
.portal-mode-toggle button.is-active{
  color:#fff;
  background:linear-gradient(135deg, var(--accent), #149a90);
  box-shadow:0 10px 24px rgba(15,118,110,0.24);
}
.portal-intro{
  margin-top:18px;
  background:var(--panel-bg);
}
.portal-intro p{
  margin:10px 0 0;
  color:var(--muted);
  line-height:1.6;
}
.portal-surface{margin-top:18px}
.portal-form{display:block}
.portal-form fieldset{
  margin:0 0 18px;
  padding:20px;
  border:1px solid var(--line);
  border-radius:22px;
  background:var(--surface-strong);
  box-shadow:0 12px 30px rgba(22,44,58,0.06);
}
.portal-form legend{
  padding:0 10px;
  color:var(--hero);
  font-family:"Georgia","Times New Roman",serif;
  font-size:1.15rem;
}
.portal-section-note{
  margin:6px 0 0;
  color:var(--muted);
  line-height:1.55;
}
.portal-subsections{
  display:grid;
  gap:16px;
  margin-top:16px;
}
.portal-subsection{
  padding:18px;
  border-radius:18px;
  border:1px solid var(--tile-border);
  background:var(--section-bg);
}
.portal-subsection h3{
  margin:0 0 6px;
  font-family:"Georgia","Times New Roman",serif;
  font-size:1.05rem;
  color:var(--hero);
}
.portal-subsection p{
  margin:0;
  color:var(--muted);
  line-height:1.55;
}
.portal-field-grid{
  display:grid;
  grid-template-columns:repeat(auto-fit, minmax(230px, 1fr));
  gap:14px;
  margin-top:14px;
}
.portal-field-card{
  padding:16px;
  border-radius:18px;
  border:1px solid var(--tile-border);
  background:var(--tile-bg);
  box-shadow:var(--tile-shadow);
}
.portal-field-card > div,
.portal-field-card > fieldset{
  height:100%;
}
.portal-form fieldset > div{margin-top:14px}
.portal-form fieldset > div:first-of-type{margin-top:8px}
.portal-form label{
  display:block;
  margin-bottom:8px;
  color:var(--muted);
  font-size:0.78rem;
  font-weight:700;
  letter-spacing:0.08em;
  text-transform:uppercase;
}
.portal-form input,.portal-form select{
  width:100%;
  border:1px solid var(--field-border);
  border-radius:14px;
  background:var(--field-bg);
  color:var(--text);
  font:inherit;
  padding:12px 14px;
  box-shadow:var(--field-shadow);
}
.portal-form input:focus,.portal-form select:focus{
  outline:2px solid rgba(15,118,110,0.18);
  border-color:var(--accent);
}
.portal-form input[type=checkbox]{
  width:1.2rem;
  height:1.2rem;
  padding:0;
  margin-top:4px;
  accent-color:var(--accent);
  justify-self:end;
}
.toggle-row{
  display:grid;
  grid-template-columns:1fr auto;
  gap:12px;
  align-items:start;
}
.toggle-row label{margin:0;padding-top:2px}
.password-row{
  display:grid;
  grid-template-columns:minmax(0, 1fr) auto;
  gap:10px;
  align-items:end;
}
.password-row label{grid-column:1 / -1}
.password-toggle{
  width:auto;
  min-width:82px;
  border:1px solid rgba(15,118,110,0.22);
  border-radius:14px;
  background:var(--accent-soft);
  color:var(--text);
  font-weight:700;
  padding:12px 14px;
  cursor:pointer;
}
.de{
  border-left:4px solid var(--bad);
  padding-left:14px;
}
.em{
  margin-top:8px;
  color:var(--bad);
  font-size:0.84rem;
  line-height:1.5;
}
.portal-actions{
  display:flex;
  flex-wrap:wrap;
  gap:14px;
  align-items:center;
  justify-content:space-between;
  padding-top:8px;
}
.portal-button-row{
  display:flex;
  flex-wrap:wrap;
  gap:12px;
  margin-left:auto;
}
.portal-help{
  margin:0;
  max-width:32rem;
  color:var(--muted);
  line-height:1.55;
}
button{
  border:0;
  border-radius:999px;
  background:linear-gradient(135deg, var(--accent), #149a90);
  color:#fff;
  cursor:pointer;
  font:inherit;
  font-weight:700;
  line-height:1;
  padding:14px 22px;
  box-shadow:0 10px 24px rgba(15,118,110,0.24);
  width:auto;
}
.portal-meta{
  margin-top:16px;
  color:var(--muted);
  font-weight:700;
}
.portal-meta-muted{font-weight:600}
.portal-meta-note{
  margin:12px 0 0;
  color:var(--muted);
  font-weight:600;
  line-height:1.6;
}
.upload-form{
  display:grid;
  gap:16px;
  margin-top:18px;
}
.upload-field{
  padding:18px;
  border-radius:18px;
  border:1px solid var(--tile-border);
  background:var(--tile-bg);
  box-shadow:var(--tile-shadow);
}
.upload-field h3{
  margin:0 0 8px;
  font-family:"Georgia","Times New Roman",serif;
  font-size:1.1rem;
  color:var(--hero);
}
.upload-field p{
  margin:0;
  color:var(--muted);
  line-height:1.6;
}
.upload-field input[type=file]{
  margin-top:14px;
  padding:14px;
}
.upload-field input[type=file]::file-selector-button{
  border:0;
  border-radius:999px;
  background:linear-gradient(135deg, var(--accent), #149a90);
  color:#fff;
  cursor:pointer;
  font:inherit;
  font-weight:700;
  margin-right:12px;
  padding:10px 16px;
}
.upload-list{
  margin:14px 0 0;
  padding-left:20px;
  color:var(--muted);
  line-height:1.7;
}
.upload-list li + li{
  margin-top:8px;
}
.upload-actions{
  display:flex;
  flex-wrap:wrap;
  gap:12px;
  align-items:center;
}
.upload-actions button{
  width:auto;
}
.upload-popup{
  position:fixed;
  inset:0;
  display:none;
  align-items:center;
  justify-content:center;
  padding:20px;
  background:rgba(17,34,45,0.42);
  backdrop-filter:blur(8px);
  z-index:30;
}
.upload-popup.is-visible{
  display:flex;
}
.upload-popup-card{
  width:min(100%, 540px);
  padding:24px;
  border-radius:24px;
  border:1px solid var(--hero-chip-border);
  box-shadow:0 18px 45px rgba(22,44,58,0.18);
}
.upload-popup-card h3{
  margin:0 0 8px;
  font-family:"Georgia","Times New Roman",serif;
  font-size:1.45rem;
  color:var(--hero);
}
.upload-popup-card p{
  margin:0;
  color:var(--muted);
  line-height:1.6;
}
.upload-progress{
  margin-top:18px;
}
.upload-progress-track{
  overflow:hidden;
  height:14px;
  border-radius:999px;
  background:rgba(23,55,72,0.12);
  box-shadow:inset 0 1px 3px rgba(22,44,58,0.08);
}
.upload-progress-bar{
  display:block;
  height:100%;
  width:0;
  border-radius:999px;
  background:linear-gradient(135deg, var(--accent), #149a90);
  box-shadow:0 8px 18px rgba(15,118,110,0.2);
  transition:width 180ms ease;
}
.upload-progress-label{
  margin-top:10px;
  font-weight:700;
  color:var(--hero);
}
.upload-progress-meta{
  margin-top:8px;
  color:var(--muted);
  font-size:0.92rem;
  line-height:1.55;
}
.upload-dismiss{
  margin-top:18px;
  color:var(--ghost-button-text);
  background:var(--ghost-button-bg);
  border:1px solid var(--ghost-button-border);
  box-shadow:0 10px 24px rgba(22,44,58,0.08);
}
.console-toolbar{
  display:flex;
  flex-wrap:wrap;
  gap:10px;
  margin-top:18px;
}
.console-toolbar button{
  width:auto;
}
.console-page-grid{
  grid-template-columns:minmax(0, 1fr);
}
.console-view{
  min-height:420px;
  max-height:68vh;
  overflow:auto;
  margin-top:18px;
  padding:18px;
  border-radius:20px;
  border:1px solid var(--tile-border);
  background:var(--console-bg);
  color:var(--console-text);
  box-shadow:inset 0 1px 2px rgba(255,255,255,0.04);
  font-family:"Courier New","SFMono-Regular",monospace;
  font-size:0.92rem;
  line-height:1.5;
  white-space:pre-wrap;
  word-break:break-word;
}
.console-ansi-bold{font-weight:700}
.console-ansi-gray{color:#9fb0b9}
.console-ansi-red{color:#ff8d8d}
.console-ansi-green{color:#8fe3a0}
.console-ansi-yellow{color:#ffd27a}
.console-ansi-blue{color:#8db8ff}
.console-ansi-magenta{color:#e2a6ff}
.console-ansi-cyan{color:#88e4ff}
.console-ansi-white{color:#f4fbff}
.console-status{
  margin-top:12px;
  color:var(--muted);
  font-weight:700;
}
.gps-raw-view{
  min-height:260px;
  max-height:42vh;
}
.gps-actions{
  margin-top:18px;
}
.console-actions{
  display:flex;
  flex-wrap:wrap;
  gap:12px;
  margin-top:18px;
}
.console-command-form{
  display:grid;
  gap:12px;
  margin-top:18px;
}
.console-command-row{
  display:grid;
  grid-template-columns:minmax(0, 1fr) auto;
  gap:12px;
  align-items:end;
}
.console-command-row input{
  width:100%;
  border:1px solid var(--field-border);
  border-radius:14px;
  background:var(--field-bg);
  color:var(--text);
  font:inherit;
  padding:12px 14px;
  box-shadow:var(--field-shadow);
}
.console-command-row input:focus{
  outline:2px solid rgba(15,118,110,0.18);
  border-color:var(--accent);
}
.console-note{
  margin-top:12px;
  color:var(--muted);
  line-height:1.6;
}
@media (max-width: 720px){
  .hero,.portal-hero{padding:22px}
  .kv-row{grid-template-columns:1fr;gap:6px}
  .kv-row dd{text-align:left}
  .dashboard-clamp{
    display:block;
    -webkit-line-clamp:unset;
    -webkit-box-orient:initial;
    overflow:visible;
    white-space:normal;
  }
  .portal-actions{justify-content:flex-start}
  .portal-button-row{width:100%;margin-left:0}
  .portal-button-row .button-link{width:100%}
  .upload-actions button{width:100%}
  .upload-popup{padding:14px}
  button{width:100%}
  .password-row{grid-template-columns:1fr}
  .password-toggle{width:100%}
  .console-command-row{grid-template-columns:1fr}
  .onboarding-actions{justify-content:flex-start}
  .onboarding-actions .spacer{display:none}
}
)clockcss";

const char kPortalConsoleCss[] PROGMEM = R"clockcss(
.portal-nav a{
  padding:10px 14px;
  border-radius:999px;
  background:var(--soft-chip-bg);
  border:1px solid var(--soft-chip-border);
  color:var(--soft-chip-text);
  font-weight:700;
  box-shadow:0 8px 18px rgba(22,44,58,0.08);
}
.portal-intro{
  margin-top:18px;
  background:var(--panel-bg);
}
.portal-intro p{
  margin:10px 0 0;
  color:var(--muted);
  line-height:1.6;
}
.portal-surface{margin-top:18px}
.portal-form{display:block}
.portal-form fieldset{
  margin:0 0 18px;
  padding:20px;
  border:1px solid var(--line);
  border-radius:22px;
  background:var(--surface-strong);
  box-shadow:0 12px 30px rgba(22,44,58,0.06);
}
.portal-form legend{
  padding:0 10px;
  color:var(--hero);
  font-family:"Georgia","Times New Roman",serif;
  font-size:1.15rem;
}
.portal-section-note{
  margin:6px 0 0;
  color:var(--muted);
  line-height:1.55;
}
.portal-subsections{
  display:grid;
  gap:16px;
  margin-top:16px;
}
.portal-subsection{
  padding:18px;
  border-radius:18px;
  border:1px solid var(--tile-border);
  background:var(--section-bg);
}
.portal-subsection h3{
  margin:0 0 6px;
  font-family:"Georgia","Times New Roman",serif;
  font-size:1.05rem;
  color:var(--hero);
}
.portal-subsection p{
  margin:0;
  color:var(--muted);
  line-height:1.55;
}
.portal-field-grid{
  display:grid;
  grid-template-columns:repeat(auto-fit, minmax(230px, 1fr));
  gap:14px;
  margin-top:14px;
}
.portal-field-card{
  padding:16px;
  border-radius:18px;
  border:1px solid var(--tile-border);
  background:var(--tile-bg);
  box-shadow:var(--tile-shadow);
}
.portal-field-card > div,
.portal-field-card > fieldset{
  height:100%;
}
.portal-form fieldset > div{margin-top:14px}
.portal-form fieldset > div:first-of-type{margin-top:8px}
.portal-form label{
  display:block;
  margin-bottom:8px;
  color:var(--muted);
  font-size:0.78rem;
  font-weight:700;
  letter-spacing:0.08em;
  text-transform:uppercase;
}
.portal-form input,.portal-form select{
  width:100%;
  border:1px solid var(--field-border);
  border-radius:14px;
  background:var(--field-bg);
  color:var(--text);
  font:inherit;
  padding:12px 14px;
  box-shadow:var(--field-shadow);
}
.portal-form input:focus,.portal-form select:focus{
  outline:2px solid rgba(15,118,110,0.18);
  border-color:var(--accent);
}
.portal-form input[type=checkbox]{
  width:1.2rem;
  height:1.2rem;
  padding:0;
  margin-top:4px;
  accent-color:var(--accent);
  justify-self:end;
}
.toggle-row{
  display:grid;
  grid-template-columns:1fr auto;
  gap:12px;
  align-items:start;
}
.toggle-row label{margin:0;padding-top:2px}
.password-row{
  display:grid;
  grid-template-columns:minmax(0, 1fr) auto;
  gap:10px;
  align-items:end;
}
.password-row label{grid-column:1 / -1}
.password-toggle{
  width:auto;
  min-width:82px;
  border:1px solid rgba(15,118,110,0.22);
  border-radius:14px;
  background:var(--accent-soft);
  color:var(--text);
  font-weight:700;
  padding:12px 14px;
  cursor:pointer;
}
.de{
  border-left:4px solid var(--bad);
  padding-left:14px;
}
.em{
  margin-top:8px;
  color:var(--bad);
  font-size:0.84rem;
  line-height:1.5;
}
.portal-actions{
  display:flex;
  flex-wrap:wrap;
  gap:14px;
  align-items:center;
  justify-content:space-between;
  padding-top:8px;
}
.portal-button-row{
  display:flex;
  flex-wrap:wrap;
  gap:12px;
  margin-left:auto;
}
.portal-help{
  margin:0;
  max-width:32rem;
  color:var(--muted);
  line-height:1.55;
}
.portal-meta{
  margin-top:16px;
  color:var(--muted);
  font-weight:700;
}
.portal-meta-muted{font-weight:600}
.portal-meta-note{
  margin:12px 0 0;
  color:var(--muted);
  font-weight:600;
  line-height:1.6;
}
.console-toolbar{
  display:flex;
  flex-wrap:wrap;
  gap:10px;
  margin-top:18px;
}
.console-toolbar button{
  width:auto;
}
.console-page-grid{
  grid-template-columns:minmax(0, 1fr);
}
.console-view{
  min-height:420px;
  max-height:68vh;
  overflow:auto;
  margin-top:18px;
  padding:18px;
  border-radius:20px;
  border:1px solid var(--tile-border);
  background:var(--console-bg);
  color:var(--console-text);
  box-shadow:inset 0 1px 2px rgba(255,255,255,0.04);
  font-family:"Courier New","SFMono-Regular",monospace;
  font-size:0.92rem;
  line-height:1.5;
  white-space:pre-wrap;
  word-break:break-word;
}
.console-status{
  margin-top:12px;
  color:var(--muted);
  font-weight:700;
}
.console-actions{
  display:flex;
  flex-wrap:wrap;
  gap:12px;
  margin-top:18px;
}
.console-command-form{
  display:grid;
  gap:12px;
  margin-top:18px;
}
.console-command-row{
  display:grid;
  grid-template-columns:minmax(0, 1fr) auto;
  gap:12px;
  align-items:end;
}
.console-command-row input{
  width:100%;
  border:1px solid var(--field-border);
  border-radius:14px;
  background:var(--field-bg);
  color:var(--text);
  font:inherit;
  padding:12px 14px;
  box-shadow:var(--field-shadow);
}
.console-command-row input:focus{
  outline:2px solid rgba(15,118,110,0.18);
  border-color:var(--accent);
}
.console-note{
  margin-top:12px;
  color:var(--muted);
  line-height:1.6;
}
@media (max-width: 720px){
  .portal-mode-bar{align-items:flex-start}
  .portal-actions{justify-content:flex-start}
  .portal-button-row{width:100%;margin-left:0}
  .portal-button-row .button-link{width:100%}
  .password-row{grid-template-columns:1fr}
  .password-toggle{width:100%}
  .console-command-row{grid-template-columns:1fr}
}
)clockcss";

const char kLiveRefreshScript[] PROGMEM = R"clockjs(
(function(){
  const intervalFallback = 15000;
  let timerId = 0;
  let inFlight = false;

  function currentShell(){
    return document.getElementById('live-shell');
  }

  function scheduleNext(){
    const shell = currentShell();
    if (!shell || document.hidden)
      return;

    const interval = Number(shell.dataset.refreshInterval || intervalFallback);
    window.clearTimeout(timerId);
    timerId = window.setTimeout(refreshShell, interval);
  }

  async function refreshShell(){
    const shell = currentShell();
    if (!shell || inFlight)
      return;

    const refreshPath = shell.dataset.refreshPath;
    if (!refreshPath)
      return;

    inFlight = true;
    try{
      const response = await fetch(refreshPath, {
        cache: 'no-store',
        headers: {'X-Requested-With': 'ledsmartclock-live'}
      });
      if (!response.ok)
        throw new Error('HTTP ' + response.status);

      const markup = (await response.text()).trim();
      if (!markup)
        throw new Error('Empty response');

      const container = document.createElement('div');
      container.innerHTML = markup;
      const nextShell = container.firstElementChild;
      const existingShell = currentShell();
      if (!existingShell || !nextShell || nextShell.id !== 'live-shell')
        throw new Error('Invalid live fragment');

      existingShell.replaceWith(nextShell);
    } catch (error){
      console.warn('LED Smart Clock live refresh failed', error);
    } finally {
      inFlight = false;
      scheduleNext();
    }
  }

  document.addEventListener('visibilitychange', function(){
    if (document.hidden)
      window.clearTimeout(timerId);
    else
      scheduleNext();
  });

  scheduleNext();
})();
)clockjs";

const char kConfigPortalScript[] PROGMEM = R"clockjs(
(function () {
  function initPortalConfigPage() {
  var nav = document.getElementById('portal-nav-links');
  var runtimeMeta = document.getElementById('portal-runtime');
  var modeButtons = Array.prototype.slice.call(document.querySelectorAll('[data-config-view]'));
  var modeStorageKey = 'ledsmartclock-config-mode';
  var basicVisibleGroups = ['iwcSys', 'Display', 'Clock', 'CurrentTemp', 'CurrentWeather', 'DailyWeather', 'Alerts', 'Status'];
  var basicVisibleFields = {
    iwcThingName: true,
    iwcWifiSsid: true,
    iwcWifiPassword: true,
    web_password_protection: true,
    web_dark_mode: true,
    iwcApPassword: true,
    ipgeoapi: true,
    weatherapi: true,
    imperial: true,
    brightness_level: true,
    text_scroll_speed: true,
    enable_alertflash: true,
    show_date: true,
    date_interval: true,
    twelve_clock: true,
    enable_fixed_tz: true,
    fixed_offset: true,
    show_current_temp: true,
    current_temp_interval: true,
    current_temp_duration: true,
    show_current_weather: true,
    current_weather_interval: true,
    current_weather_short_text: true,
    show_daily_weather: true,
    daily_weather_interval: true,
    daily_weather_short_text: true,
    show_aqi: true,
    aqi_interval: true,
    alert_interval: true,
    enable_system_status: true,
    enable_aqi_status: true,
    enable_uvi_status: true,
    green_status: true
  };
  var sectionNotes = {
    iwcSys: 'Network identity, credentials, web theme, API keys, and maintenance controls.',
    Display: 'Brightness, scrolling, colors, and date messaging.',
    Clock: 'Clock format, timezone fallback, and clock-face appearance.',
    CurrentTemp: 'Temperature display cadence and custom coloring.',
    CurrentWeather: 'Current-conditions scrolling and presentation timing.',
    DailyWeather: 'Forecast and AQI summaries used during the daily rotation.',
    Alerts: 'Alert polling cadence and interrupt timing.',
    Status: 'Corner status pixels that summarize system, AQI, and UV state. Turn any of the three corners off entirely here.',
    Sun: 'Sunrise and sunset messages based on local solar events.',
    Location: 'Coordinate overrides and location-change messaging.',
    GPS: 'Receiver-specific troubleshooting controls such as UART baud and hardware recovery.'
  };

  if (runtimeMeta && runtimeMeta.dataset.ntpServer) {
    sectionNotes.Clock += ' Currently using ' + runtimeMeta.dataset.ntpServer +
      ' via ' + (runtimeMeta.dataset.ntpSource || 'the current selection') + '.';
  }

  function buildFieldCard(node) {
    var card = document.createElement('div');
    card.className = 'portal-field-card';
    var control = node.querySelector ? node.querySelector('[id]') : null;
    if (control && control.id) {
      card.dataset.configId = control.id;
    }
    card.appendChild(node);
    return card;
  }

  function wrapFieldsetRows(fieldset) {
    if (!fieldset || fieldset.dataset.rowsWrapped === 'true') {
      return;
    }

    var nodes = Array.from(fieldset.children).filter(function (child) {
      return child.tagName !== 'LEGEND' &&
        !child.classList.contains('portal-section-note') &&
        !child.classList.contains('portal-subsections') &&
        !child.classList.contains('portal-field-grid');
    });

    if (!nodes.length) {
      fieldset.dataset.rowsWrapped = 'true';
      return;
    }

    var grid = document.createElement('div');
    grid.className = 'portal-field-grid';
    nodes.forEach(function (node) {
      grid.appendChild(buildFieldCard(node));
    });
    fieldset.appendChild(grid);
    fieldset.dataset.rowsWrapped = 'true';
  }

  function addSectionNote(group) {
    if (!group || group.dataset.noteAdded === 'true') {
      return;
    }
    var legend = group.querySelector('legend');
    if (!legend) {
      return;
    }
    var noteText = sectionNotes[group.id];
    if (!noteText) {
      return;
    }
    var note = document.createElement('p');
    note.className = 'portal-section-note';
    note.textContent = noteText;
    legend.insertAdjacentElement('afterend', note);
    group.dataset.noteAdded = 'true';
  }

  function findWrapper(id) {
    var field = document.getElementById(id);
    return field && field.parentElement ? field.parentElement : null;
  }

  function enhanceSystemGroup() {
    var systemGroup = document.getElementById('iwcSys');
    if (!systemGroup || systemGroup.dataset.portalStructured === 'true') {
      return;
    }

    var sections = [
      {
        title: 'Connectivity & Access',
        description: 'How you reach the clock, secure the portal, choose the web theme, and keep the device discoverable.',
        nodes: [
          findWrapper('iwcThingName'),
          findWrapper('iwcWifiSsid'),
          findWrapper('iwcWifiPassword'),
          findWrapper('web_password_protection'),
          findWrapper('web_dark_mode'),
          findWrapper('iwcApPassword'),
          findWrapper('iwcApTimeout')
        ]
      },
      {
        title: 'Cloud Services',
        description: 'External APIs used for automatic timezone, weather, forecast, AQI, and reverse geocoding.',
        nodes: [
          findWrapper('ipgeoapi'),
          findWrapper('weatherapi')
        ]
      },
      {
        title: 'Maintenance',
        description: 'Troubleshooting and recovery controls that are only needed occasionally.',
        nodes: [
          findWrapper('serialdebug'),
          findWrapper('resetdefaults')
        ]
      }
    ];

    var movedNodes = [];
    var sectionContainer = document.createElement('div');
    sectionContainer.className = 'portal-subsections';

    sections.forEach(function (sectionConfig) {
      var nodes = sectionConfig.nodes.filter(function (node) {
        return !!node;
      });

      if (!nodes.length) {
        return;
      }

      var section = document.createElement('section');
      section.className = 'portal-subsection';
      var heading = document.createElement('h3');
      heading.textContent = sectionConfig.title;
      var description = document.createElement('p');
      description.textContent = sectionConfig.description;
      var grid = document.createElement('div');
      grid.className = 'portal-field-grid';

      nodes.forEach(function (node) {
        movedNodes.push(node);
        grid.appendChild(buildFieldCard(node));
      });

      section.appendChild(heading);
      section.appendChild(description);
      section.appendChild(grid);
      sectionContainer.appendChild(section);
    });

    Array.from(systemGroup.children).forEach(function (child) {
      if (child.tagName === 'LEGEND' || child.classList.contains('portal-section-note')) {
        return;
      }
      if (movedNodes.indexOf(child) === -1) {
        var fallbackGrid = sectionContainer.querySelector('.portal-subsection:last-child .portal-field-grid');
        if (fallbackGrid) {
          fallbackGrid.appendChild(buildFieldCard(child));
        }
      }
    });

    systemGroup.appendChild(sectionContainer);
    systemGroup.dataset.portalStructured = 'true';
  }

  function applyConfigMode(mode) {
    var effectiveMode = mode === 'advanced' ? 'advanced' : 'basic';

    document.querySelectorAll('.portal-form > fieldset[id]').forEach(function (group) {
      var groupVisible = effectiveMode === 'advanced' || basicVisibleGroups.indexOf(group.id) !== -1;
      group.hidden = !groupVisible;
      if (!groupVisible) {
        return;
      }

      group.querySelectorAll('.portal-field-card').forEach(function (card) {
        var configId = card.dataset.configId || '';
        var visible = effectiveMode === 'advanced' || !!basicVisibleFields[configId];
        card.hidden = !visible;
      });

      group.querySelectorAll('.portal-subsection').forEach(function (section) {
        var visibleCards = Array.prototype.some.call(section.querySelectorAll('.portal-field-card'), function (card) {
          return !card.hidden;
        });
        section.hidden = !visibleCards;
      });

      var visibleStandaloneCards = Array.prototype.some.call(group.children, function (child) {
        if (!child.classList || !child.classList.contains('portal-field-grid')) {
          return false;
        }
        return Array.prototype.some.call(child.querySelectorAll('.portal-field-card'), function (card) {
          return !card.hidden;
        });
      });
      var visibleSubsections = Array.prototype.some.call(group.querySelectorAll('.portal-subsection'), function (section) {
        return !section.hidden;
      });
      group.hidden = !visibleStandaloneCards && !visibleSubsections && group.id !== 'iwcSys';
    });

    modeButtons.forEach(function (button) {
      button.classList.toggle('is-active', button.dataset.configView === effectiveMode);
    });

    if (runtimeMeta) {
      runtimeMeta.dataset.configMode = effectiveMode;
    }
  }

  function syncPortalPasswordProtection() {
    var protectionToggle = document.getElementById('web_password_protection');
    var passwordField = document.getElementById('iwcApPassword');
    if (!protectionToggle || !passwordField) {
      return;
    }

    var passwordCard = passwordField.closest('.portal-field-card') || passwordField.parentElement;
    if (!passwordCard) {
      return;
    }

    passwordCard.hidden = !protectionToggle.checked;
  }

  document.querySelectorAll('.portal-form > fieldset[id]').forEach(function (group) {
    addSectionNote(group);
  });

  enhanceSystemGroup();

  document.querySelectorAll('.portal-form > fieldset[id]').forEach(function (group) {
    if (group.id !== 'iwcSys') {
      wrapFieldsetRows(group);
    }
  });

  document.querySelectorAll(".portal-form input[type='checkbox']").forEach(function (input) {
    if (input.parentElement) {
      input.parentElement.classList.add('toggle-row');
    }
  });

  document.querySelectorAll(".portal-form input[type='password']").forEach(function (input) {
    var wrapper = input.parentElement;
    if (!wrapper || wrapper.dataset.passwordEnhanced === 'true') {
      return;
    }
    wrapper.dataset.passwordEnhanced = 'true';
    wrapper.classList.add('password-row');

    var button = document.createElement('button');
    button.type = 'button';
    button.className = 'password-toggle';
    button.textContent = 'Show';
    button.addEventListener('click', function () {
      var hidden = input.type === 'password';
      input.type = hidden ? 'text' : 'password';
      button.textContent = hidden ? 'Hide' : 'Show';
    });
    wrapper.appendChild(button);
  });

  var protectionToggle = document.getElementById('web_password_protection');
  if (protectionToggle && protectionToggle.dataset.portalBound !== 'true') {
    protectionToggle.dataset.portalBound = 'true';
    protectionToggle.addEventListener('change', syncPortalPasswordProtection);
  }
  syncPortalPasswordProtection();

  if (!nav) {
    return;
  }

  nav.innerHTML = '';
  nav.style.display = '';

  document.querySelectorAll('.portal-form > fieldset[id]').forEach(function (group) {
    var legend = group.firstElementChild;
    if (!legend || legend.tagName !== 'LEGEND') {
      legend = group.querySelector('legend');
    }
    if (!legend) {
      return;
    }
    var link = document.createElement('a');
    link.href = '#' + group.id;
    link.textContent = legend.textContent;
    nav.appendChild(link);
  });

  if (!nav.children.length) {
    nav.style.display = 'none';
  }

  modeButtons.forEach(function (button) {
    if (button.dataset.modeBound === 'true') {
      return;
    }
    button.dataset.modeBound = 'true';
    button.addEventListener('click', function () {
      var mode = button.dataset.configView === 'advanced' ? 'advanced' : 'basic';
      try {
        window.localStorage.setItem(modeStorageKey, mode);
      } catch (error) {
      }
      applyConfigMode(mode);
      syncPortalPasswordProtection();
    });
  });

  var initialMode = 'basic';
  try {
    if (window.localStorage.getItem(modeStorageKey) === 'advanced') {
      initialMode = 'advanced';
    }
  } catch (error) {
  }
  applyConfigMode(initialMode);
  syncPortalPasswordProtection();
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initPortalConfigPage, { once: true });
  } else {
    initPortalConfigPage();
  }
  window.addEventListener('pageshow', initPortalConfigPage);
})();
)clockjs";

const char kOnboardingScript[] PROGMEM = R"clockjs(
document.addEventListener('DOMContentLoaded', function () {
  var steps = Array.prototype.slice.call(document.querySelectorAll('.onboarding-step[data-step]'));
  var protectionMode = document.getElementById('web_protection');
  var adminPassword = document.getElementById('admin_password');
  if (!steps.length) {
    return;
  }

  var currentStep = 1;
  var totalSteps = steps.length;

  function showStep(stepNumber) {
    currentStep = Math.max(1, Math.min(totalSteps, stepNumber));
    steps.forEach(function (step) {
      var matches = Number(step.dataset.step || '1') === currentStep;
      step.hidden = !matches;
    });

    document.querySelectorAll('[data-step-pill]').forEach(function (pill) {
      var active = Number(pill.dataset.stepPill || '1') === currentStep;
      pill.classList.toggle('tone-good', active);
    });
  }

  document.querySelectorAll('[data-next-step]').forEach(function (button) {
    button.addEventListener('click', function () {
      showStep(Number(button.dataset.nextStep || String(currentStep + 1)));
    });
  });

  document.querySelectorAll('[data-prev-step]').forEach(function (button) {
    button.addEventListener('click', function () {
      showStep(Number(button.dataset.prevStep || String(currentStep - 1)));
    });
  });

  function syncProtectionFields() {
    if (!protectionMode || !adminPassword || !adminPassword.parentElement) {
      return;
    }
    var enabled = protectionMode.value !== 'disabled';
    adminPassword.parentElement.hidden = !enabled;
    adminPassword.required = enabled;
    adminPassword.minLength = enabled ? 8 : 0;
  }

  if (protectionMode) {
    protectionMode.addEventListener('change', syncProtectionFields);
  }

  syncProtectionFields();
  showStep(1);
});
)clockjs";

const char kFirmwareUploadScript[] PROGMEM = R"clockjs(
document.addEventListener('DOMContentLoaded', function () {
  var form = document.querySelector('.upload-form');
  if (!form) {
    return;
  }

  var fileInput = document.getElementById('firmware-upload');
  var submitButton = form.querySelector("button[type='submit']");
  var popup = document.getElementById('upload-popup');
  var popupCard = document.getElementById('upload-popup-card');
  var title = document.getElementById('upload-popup-title');
  var text = document.getElementById('upload-popup-text');
  var progress = document.getElementById('upload-progress');
  var progressBar = document.getElementById('upload-progress-bar');
  var progressLabel = document.getElementById('upload-progress-label');
  var progressMeta = document.getElementById('upload-progress-meta');
  var dismissButton = document.getElementById('upload-dismiss');

  function setBusy(busy) {
    if (submitButton) {
      submitButton.disabled = busy;
      submitButton.textContent = busy ? 'Uploading…' : 'Upload Firmware';
    }
    if (fileInput) {
      fileInput.disabled = busy;
    }
  }

  function setToneClass(toneClass) {
    popupCard.classList.remove('tone-neutral', 'tone-good', 'tone-warn', 'tone-bad');
    popupCard.classList.add(toneClass);
  }

  function showPopup(toneClass, heading, message, percent, meta, showDismiss) {
    setToneClass(toneClass);
    popup.classList.add('is-visible');
    popup.hidden = false;
    title.textContent = heading;
    text.textContent = message;

    if (typeof percent === 'number') {
      progress.hidden = false;
      progressBar.style.width = Math.max(0, Math.min(100, percent)) + '%';
      progressLabel.textContent = Math.round(percent) + '% transferred';
    } else {
      progress.hidden = true;
      progressBar.style.width = '0%';
      progressLabel.textContent = '';
    }

    progressMeta.textContent = meta || '';
    dismissButton.hidden = !showDismiss;
  }

  function hidePopup() {
    popup.classList.remove('is-visible');
    popup.hidden = true;
  }

  dismissButton.addEventListener('click', function () {
    hidePopup();
  });

  form.addEventListener('submit', function (event) {
    event.preventDefault();

    if (!fileInput.files || fileInput.files.length === 0) {
      showPopup(
        'tone-bad',
        'No firmware selected',
        'Choose the compiled update.bin file before starting the OTA update.',
        null,
        'The upload has not started.',
        true
      );
      return;
    }

    var file = fileInput.files[0];
    var lowerName = file.name.toLowerCase();
    if (!lowerName.endsWith('.bin')) {
      showPopup(
        'tone-bad',
        'Unsupported file type',
        'This screen only accepts the compiled update.bin image.',
        null,
        'Select the OTA application image and try again.',
        true
      );
      return;
    }

    if (lowerName !== 'update.bin') {
      showPopup(
        'tone-bad',
        'Wrong firmware image',
        'This OTA page only accepts update.bin.',
        null,
        'Do not upload firmware.bin here. That file is for first-time installs through the web installer or USB flashing.',
        true
      );
      return;
    }

    var data = new FormData(form);
    var xhr = new XMLHttpRequest();
    xhr.open('POST', form.action, true);
    xhr.responseType = 'text';
    xhr.timeout = 240000;

    setBusy(true);
    showPopup(
      'tone-neutral',
      'Uploading firmware',
      'The browser is sending the new image to the clock. Keep this page open until the device finishes processing it.',
      0,
      'Large firmware images can take a bit to transfer over Wi-Fi.',
      false
    );

    xhr.upload.addEventListener('progress', function (progressEvent) {
      if (!progressEvent.lengthComputable) {
        progress.hidden = false;
        progressLabel.textContent = 'Uploading firmware…';
        progressMeta.textContent = 'The browser could not determine total size, but the transfer is still active.';
        return;
      }

      var percent = (progressEvent.loaded / progressEvent.total) * 100;
      showPopup(
        'tone-neutral',
        'Uploading firmware',
        'The image is still transferring to the clock.',
        percent,
        file.name + ' • ' + Math.round(file.size / 1024) + ' KB',
        false
      );
    });

    xhr.upload.addEventListener('load', function () {
      showPopup(
        'tone-warn',
        'Finishing update',
        'The transfer reached the clock. It is now validating the image, writing flash, and preparing a reboot if the update succeeds.',
        100,
        'Do not close the page yet. The device still needs a few moments to finish.',
        false
      );
    });

    xhr.addEventListener('load', function () {
      if (xhr.responseText && xhr.responseText.length > 0) {
        document.open('text/html', 'replace');
        document.write(xhr.responseText);
        document.close();
        return;
      }

      setBusy(false);
      showPopup(
        xhr.status >= 200 && xhr.status < 300 ? 'tone-good' : 'tone-bad',
        xhr.status >= 200 && xhr.status < 300 ? 'Update finished' : 'Upload failed',
        xhr.status >= 200 && xhr.status < 300
          ? 'The clock accepted the upload.'
          : 'The clock returned an empty response after the upload.',
        xhr.status >= 200 && xhr.status < 300 ? 100 : null,
        'HTTP status ' + xhr.status,
        true
      );
    });

    xhr.addEventListener('error', function () {
      setBusy(false);
      showPopup(
        'tone-bad',
        'Connection lost',
        'The browser lost contact with the clock before the upload completed.',
        null,
        'The device did not confirm success. Retry the upload after the clock is reachable again.',
        true
      );
    });

    xhr.addEventListener('abort', function () {
      setBusy(false);
      showPopup(
        'tone-bad',
        'Upload cancelled',
        'The firmware upload was aborted before completion.',
        null,
        'No reboot was requested.',
        true
      );
    });

    xhr.addEventListener('timeout', function () {
      setBusy(false);
      showPopup(
        'tone-bad',
        'Upload timed out',
        'The browser waited too long for the clock to finish the update.',
        null,
        'If the device is still reachable, refresh the page and check whether the firmware already changed.',
        true
      );
    });

    xhr.send(data);
  });
});
)clockjs";

const char kConsolePageScript[] PROGMEM = R"clockjs(
document.addEventListener('DOMContentLoaded', function () {
  var page = document.getElementById('console-page');
  var output = document.getElementById('console-output');
  var status = document.getElementById('console-status');
  var form = document.getElementById('console-command-form');
  var input = document.getElementById('console-command');
  var accessToken = page && page.dataset.consoleToken ? page.dataset.consoleToken : '';
  var cursor = Number(output && output.dataset.cursor ? output.dataset.cursor : '0');
  var pollTimer = null;
  var pollInFlight = false;

  function withToken(path) {
    if (!accessToken) {
      return path;
    }
    return path + (path.indexOf('?') === -1 ? '?' : '&') + 'token=' + encodeURIComponent(accessToken);
  }

  function setStatus(text) {
    if (status) {
      status.textContent = text;
    }
  }

  function stripAnsi(text) {
    if (!text) {
      return '';
    }
    return text.replace(/\u001b\[[0-9;]*m/g, '');
  }

  function appendOutput(text) {
    if (!output || !text) {
      return;
    }
    var wasNearBottom = (output.scrollTop + output.clientHeight + 40) >= output.scrollHeight;
    output.textContent += stripAnsi(text);
    if (wasNearBottom) {
      output.scrollTop = output.scrollHeight;
    }
  }

  function schedulePoll(delayMs) {
    if (pollTimer) {
      window.clearTimeout(pollTimer);
    }
    pollTimer = window.setTimeout(fetchConsole, delayMs);
  }

  function fetchConsole() {
    if (!output || pollInFlight || document.hidden) {
      return;
    }

    pollInFlight = true;
    fetch(withToken('/console/log?since=' + encodeURIComponent(String(cursor))), {
      cache: 'no-store',
      credentials: 'same-origin'
    })
      .then(function (response) {
        if (!response.ok) {
          return response.text().then(function (body) {
            throw new Error('HTTP ' + response.status + (body ? ': ' + body : ''));
          });
        }
        var nextCursor = Number(response.headers.get('X-Console-Cursor') || cursor);
        var truncated = response.headers.get('X-Console-Truncated') === '1';
        return response.text().then(function (text) {
          if (truncated) {
            output.textContent = '';
            appendOutput('[console buffer wrapped; showing newest available output]\n');
          }
          if (text) {
            appendOutput(text);
          }
          cursor = nextCursor;
          setStatus('Live console connected. Cursor ' + cursor + '.');
        });
      })
      .catch(function (error) {
        setStatus('Console polling failed: ' + error.message);
      })
      .finally(function () {
        pollInFlight = false;
        schedulePoll(1500);
      });
  }

  document.querySelectorAll('[data-console-command]').forEach(function (button) {
    button.addEventListener('click', function () {
      if (!input) {
        return;
      }
      input.value = button.dataset.consoleCommand || '';
      if (form.requestSubmit) {
        form.requestSubmit();
      } else {
        form.dispatchEvent(new Event('submit', { cancelable: true }));
      }
    });
  });

  if (form) {
    form.addEventListener('submit', function (event) {
      event.preventDefault();
      if (!input) {
        return;
      }
      var command = (input.value || '').trim();
      if (!command) {
        setStatus('Enter a command before sending it.');
        return;
      }

      fetch(withToken('/console/command'), {
        method: 'POST',
        credentials: 'same-origin',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8'
        },
        body: 'cmd=' + encodeURIComponent(command) + '&token=' + encodeURIComponent(accessToken)
      }).then(function (response) {
        return response.text().then(function (body) {
          var payload = {};
          try {
            payload = body ? JSON.parse(body) : {};
          } catch (error) {
            payload = {};
          }

          if (!response.ok || payload.ok === false) {
            var message = payload.error || ('HTTP ' + response.status);
            throw new Error(message);
          }

          input.value = '';
          setStatus('Sent command "' + command + '". Waiting for console output...');
          fetchConsole();
        });
      }).catch(function (error) {
        setStatus('Command failed: ' + error.message);
      });
    });
  }

  document.querySelectorAll('[data-console-clear]').forEach(function (button) {
    button.addEventListener('click', function () {
      if (!window.confirm('Clear the retained web console buffer? This only removes the in-memory log tail.')) {
        return;
      }

      fetch(withToken('/console/clear'), {
        method: 'POST',
        credentials: 'same-origin',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8'
        },
        body: 'token=' + encodeURIComponent(accessToken)
      }).then(function (response) {
        return response.text().then(function (body) {
          var payload = {};
          try {
            payload = body ? JSON.parse(body) : {};
          } catch (error) {
            payload = {};
          }

          if (!response.ok || payload.ok === false) {
            var message = payload.error || ('HTTP ' + response.status);
            throw new Error(message);
          }

          cursor = Number(payload.cursor || 0);
          if (output) {
            output.textContent = '';
            output.dataset.cursor = String(cursor);
          }
          setStatus(payload.message || 'Console buffer cleared.');
        });
      }).catch(function (error) {
        setStatus('Clear failed: ' + error.message);
      });
    });
  });

  if (output) {
    var initialText = output.textContent || '';
    output.textContent = '';
    appendOutput(initialText);
  }

  fetchConsole();
  document.addEventListener('visibilitychange', function () {
    if (document.hidden) {
      if (pollTimer) {
        window.clearTimeout(pollTimer);
      }
      return;
    }
    fetchConsole();
  });
  window.addEventListener('beforeunload', function () {
    if (pollTimer) {
      window.clearTimeout(pollTimer);
    }
  });
});
)clockjs";

const char kDiagnosticsPageScript[] PROGMEM = R"clockjs(
document.addEventListener('DOMContentLoaded', function () {
  var rawPollTimer = null;
  var rawPollInFlight = false;
  var actionStatus = document.getElementById('diagnostics-action-status');

  function rawOutput() {
    return document.getElementById('gps-raw-output');
  }

  function rawStatus() {
    return document.getElementById('gps-raw-status');
  }

  function setRawStatus(text) {
    var status = rawStatus();
    if (status) {
      status.textContent = text;
    }
  }

  function setActionStatus(text) {
    if (actionStatus) {
      actionStatus.textContent = text;
    }
  }

  function scheduleRawPoll(delayMs) {
    if (rawPollTimer) {
      window.clearTimeout(rawPollTimer);
    }
    if (document.hidden) {
      return;
    }
    rawPollTimer = window.setTimeout(fetchRawNmea, delayMs);
  }

  function fetchRawNmea() {
    var output = rawOutput();
    if (!output || rawPollInFlight || document.hidden) {
      return;
    }

    rawPollInFlight = true;
    fetch('/gps/raw', {
      cache: 'no-store',
      credentials: 'same-origin',
      headers: {'X-Requested-With': 'ledsmartclock-gps'}
    }).then(function (response) {
      if (!response.ok) {
        return response.text().then(function (body) {
          throw new Error('HTTP ' + response.status + (body ? ': ' + body : ''));
        });
      }
      return response.text().then(function (text) {
        var wasNearBottom = (output.scrollTop + output.clientHeight + 40) >= output.scrollHeight;
        output.textContent = text || 'No raw NMEA captured yet.';
        if (wasNearBottom) {
          output.scrollTop = output.scrollHeight;
        }
        setRawStatus('Live GPS raw view connected.');
      });
    }).catch(function (error) {
      setRawStatus('GPS raw view failed: ' + error.message);
    }).finally(function () {
      rawPollInFlight = false;
      scheduleRawPoll(1500);
    });
  }

  document.addEventListener('click', function (event) {
    var displayButton = event.target.closest('[data-display-test]');
    if (displayButton) {
      event.preventDefault();

      var action = displayButton.dataset.displayTest || '';
      if (!action) {
        return;
      }

      setActionStatus('Queueing display test "' + action + '"...');
      fetch('/display-test', {
        method: 'POST',
        credentials: 'same-origin',
        headers: {
          'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8'
        },
        body: 'action=' + encodeURIComponent(action)
      }).then(function (response) {
        return response.text().then(function (body) {
          var payload = {};
          try {
            payload = body ? JSON.parse(body) : {};
          } catch (error) {
            payload = {};
          }

          if (!response.ok || payload.ok === false) {
            throw new Error(payload.error || ('HTTP ' + response.status));
          }

          setActionStatus(payload.message || ('Queued display test "' + action + '".'));
        });
      }).catch(function (error) {
        setActionStatus('Display test failed: ' + error.message);
      });
      return;
    }

    var button = event.target.closest('[data-gps-action]');
    if (!button) {
      return;
    }
    event.preventDefault();

    var action = button.dataset.gpsAction || '';
    if (!action) {
      return;
    }

    setRawStatus('Sending GPS action "' + action + '"...');
    fetch('/gps/action', {
      method: 'POST',
      credentials: 'same-origin',
      headers: {
        'Content-Type': 'application/x-www-form-urlencoded;charset=UTF-8'
      },
      body: 'action=' + encodeURIComponent(action)
    }).then(function (response) {
      return response.text().then(function (body) {
        var payload = {};
        try {
          payload = body ? JSON.parse(body) : {};
        } catch (error) {
          payload = {};
        }

        if (!response.ok || payload.ok === false) {
          throw new Error(payload.error || ('HTTP ' + response.status));
        }

        setRawStatus(payload.message || ('GPS action "' + action + '" completed.'));
        fetchRawNmea();
      });
    }).catch(function (error) {
      setRawStatus('GPS action failed: ' + error.message);
    });
  });

  document.addEventListener('visibilitychange', function () {
    if (document.hidden) {
      window.clearTimeout(rawPollTimer);
    } else {
      scheduleRawPoll(250);
    }
  });

  scheduleRawPoll(250);
});
)clockjs";

/** Returns the current portal/network state as an array-safe label. */
String currentConnectionStateLabel()
{
  return String(connection_state[static_cast<int>(iotWebConf.getState())]);
}

/** Returns true when the device is serving the captive-portal style setup flow. */
bool isSetupPortalState()
{
  iotwebconf::NetworkState state = iotWebConf.getState();
  return state == iotwebconf::NotConfigured || state == iotwebconf::ApMode;
}

/** Returns true when the device is in its normal secured online state. */
bool isProtectedPortalState()
{
  return iotWebConf.getState() == iotwebconf::OnLine && web_password_protection.isChecked();
}

/** Percent-encodes a path so it can safely round-trip through query parameters. */
String urlEncode(const String &value)
{
  String encoded;
  encoded.reserve(value.length() * 3U);

  for (size_t index = 0; index < value.length(); ++index)
  {
    const unsigned char currentChar = static_cast<unsigned char>(value[index]);
    if (isalnum(currentChar) || currentChar == '-' || currentChar == '_' || currentChar == '.' || currentChar == '~' ||
        currentChar == '/' || currentChar == '?')
    {
      encoded += static_cast<char>(currentChar);
      continue;
    }

    char hex[4];
    snprintf(hex, sizeof(hex), "%%%02X", currentChar);
    encoded += hex;
  }

  return encoded;
}

/** Extracts one cookie value from the collected Cookie header. */
String cookieValue(const char *name)
{
  if (!server.hasHeader("Cookie"))
    return String();

  const String header = server.header("Cookie");
  const String cookieName = String(name) + '=';
  int start = header.indexOf(cookieName);
  if (start < 0)
    return String();

  start += cookieName.length();
  int end = header.indexOf(';', start);
  String value = end >= 0 ? header.substring(start, end) : header.substring(start);
  value.trim();
  return value;
}

/** Returns true when the current browser session already holds a valid web-session cookie. */
bool hasValidWebSession()
{
  if (!isProtectedPortalState())
    return true;
  if (webSessionToken[0] == '\0')
    return false;
  if (static_cast<uint32_t>(millis() - webSessionIssuedAt) > (kWebSessionLifetimeSeconds * 1000UL))
    return false;

  String cookieToken = cookieValue(kWebSessionCookieName);
  return cookieToken.length() > 0 && strcmp(cookieToken.c_str(), webSessionToken) == 0;
}

/** Clears the in-memory web-session token. */
void clearWebSessionState()
{
  webSessionToken[0] == '\0';
  webSessionIssuedAt = 0;
  consoleAccessToken[0] = '\0';
  consoleAccessTokenIssuedAt = 0;
}

/** Writes a Set-Cookie header for the session token with the requested lifetime. */
void sendSessionCookie(const String &token, uint32_t maxAgeSeconds)
{
  String cookie = String(kWebSessionCookieName) + '=' + token + F("; Path=/; SameSite=Lax; HttpOnly; Max-Age=") + String(maxAgeSeconds);
  server.sendHeader("Set-Cookie", cookie);
}

/** Starts a fresh authenticated web session after the password form succeeds. */
void issueWebSession()
{
  snprintf(webSessionToken, sizeof(webSessionToken), "%08lx%08lx%08lx%08lx",
           static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()));
  webSessionIssuedAt = millis();
  sendSessionCookie(String(webSessionToken), kWebSessionLifetimeSeconds);
}

/** Clears the browser cookie for the authenticated web session. */
void clearWebSessionCookie()
{
  clearWebSessionState();
  sendSessionCookie(String(), 0);
}

/** Returns the safe post-login redirect target requested by the browser. */
String requestedNextPath()
{
  String nextPath = server.hasArg("next") ? server.arg("next") : String("/");
  if (nextPath.length() == 0 || nextPath.charAt(0) != '/' || nextPath.startsWith("//"))
    return String("/");
  return nextPath;
}

/** Redirects the browser to the supplied local path. */
void redirectTo(const String &location)
{
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Location", location, true);
  server.send(303, "text/plain", "");
}

/** Redirects unauthenticated users to the custom password login page. */
bool authorizeAdminRequest();

/** Returns the current firmware-upload target path. */
String firmwareUpdatePathLabel()
{
  return htmlEscape(String(kFirmwareUpdatePath));
}

/** Resets the per-request OTA upload state tracked by the themed firmware page. */
void resetFirmwareUploadState()
{
  firmwareUploadAuthenticated = false;
  firmwareUploadStarted = false;
  firmwareUploadError = "";
  firmwareUploadLooksLikeAppImage = false;
}

/** Resets the per-request JSON config-import state tracked by the backup page. */
void resetConfigImportState()
{
  configImportAuthenticated = false;
  configImportStarted = false;
  configImportError = "";
  configImportPayload = "";
}

/** Captures the latest OTA updater error as readable text for the themed status page. */
void setFirmwareUploadErrorFromUpdater()
{
  firmwareUploadError = String(F("Firmware update failed: ")) + String(Update.errorString());
}

/** Streams the posted firmware image into the ESP32 OTA updater. */
void handleFirmwareUpload()
{
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START)
  {
    resetFirmwareUploadState();
    Update.clearError();
    firmwareUploadAuthenticated = isAdminSessionAuthorized();

    if (!firmwareUploadAuthenticated)
    {
      firmwareUploadError = F("Authentication failed before the firmware upload could start.");
      return;
    }

    if (upload.name != "firmware")
    {
      firmwareUploadError = F("Unexpected upload field. Choose an update.bin file and try again.");
      return;
    }

    String uploadName = upload.filename;
    uploadName.toLowerCase();
    if (!uploadName.endsWith(".bin"))
    {
      firmwareUploadError = F("Only the compiled update.bin image is supported on this page.");
      return;
    }

    if (uploadName != "update.bin")
    {
      firmwareUploadError = F("Wrong firmware image. This OTA page only accepts update.bin. Use firmware.bin only for first-time installs.");
      return;
    }

    firmwareUploadStarted = true;
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000U) & 0xFFFFF000U;
    if (!Update.begin(maxSketchSpace, U_FLASH))
      setFirmwareUploadErrorFromUpdater();
  }
  else if (firmwareUploadAuthenticated && upload.status == UPLOAD_FILE_WRITE && firmwareUploadError.length() == 0)
  {
    if (!firmwareUploadLooksLikeAppImage && upload.currentSize > 0)
    {
      if (upload.buf[0] != 0xE9)
      {
        firmwareUploadError = F("This file does not look like an OTA application image. Upload update.bin, not firmware.bin.");
        return;
      }
      firmwareUploadLooksLikeAppImage = true;
    }

    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
      setFirmwareUploadErrorFromUpdater();
  }
  else if (firmwareUploadAuthenticated && upload.status == UPLOAD_FILE_END && firmwareUploadError.length() == 0)
  {
    if (!Update.end(true))
      setFirmwareUploadErrorFromUpdater();
  }
  else if (upload.status == UPLOAD_FILE_ABORTED)
  {
    if (Update.isRunning())
      Update.abort();
    firmwareUploadError = F("Firmware upload was aborted before completion.");
  }

  delay(0);
}

bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper)
{
  ESP_LOGD(TAG, "Validating web form...");
  if (webRequestWrapper != nullptr)
  {
    const bool protectionEnabled = webRequestWrapper->hasArg("web_password_protection");
    if (protectionEnabled && webRequestWrapper->hasArg("iwcApPassword"))
    {
      String submittedPassword = webRequestWrapper->arg("iwcApPassword");
      if (submittedPassword.length() < 8)
      {
        iotWebConf.getApPasswordParameter()->errorMessage = "Use at least 8 characters when web password protection is enabled.";
        return false;
      }
    }
  }
  return true;
}
