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
    if (!file.name.toLowerCase().endsWith('.bin')) {
      showPopup(
        'tone-bad',
        'Unsupported file type',
        'This screen only accepts the compiled update.bin image.',
        null,
        'Select the application image produced by PlatformIO and try again.',
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
  webSessionToken[0] = '\0';
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
void redirectToLogin(const String &nextPath)
{
  if (!isProtectedPortalState())
  {
    redirectTo(isSetupPortalState() ? String(kOnboardingPath) : String("/"));
    return;
  }

  redirectTo(String(kLoginPath) + F("?next=") + urlEncode(nextPath));
}

/** Prompts for the custom web login if the current request lacks a valid session. */
bool authorizeAdminRequest()
{
  if (!isProtectedPortalState())
    return true;
  if (hasValidWebSession())
    return true;

  redirectToLogin(server.uri());
  return false;
}

/** Returns true when a handler only needs to know if the active browser is already authenticated. */
bool isAdminSessionAuthorized()
{
  return !isProtectedPortalState() || hasValidWebSession();
}

/** Provides IotWebConf with the current server request while honoring the custom session login. */
class SessionWebRequestWrapper : public iotwebconf::WebRequestWrapper
{
public:
  const String hostHeader() const override { return server.hostHeader(); }
  IPAddress localIP() override { return server.client().localIP(); }
  uint16_t localPort() override { return server.client().localPort(); }
  const String uri() const override { return server.uri(); }
  bool authenticate(const char *username, const char *password) override
  {
    (void)username;
    (void)password;
    return isAdminSessionAuthorized();
  }
  void requestAuthentication() override { redirectToLogin(server.uri()); }
  bool hasArg(const String &name) override { return server.hasArg(name); }
  String arg(const String name) override { return server.arg(name); }
  void sendHeader(const String &name, const String &value, bool first = false) override { server.sendHeader(name, value, first); }
  void setContentLength(const size_t contentLength) override { server.setContentLength(contentLength); }
  void send(int code, const char *content_type = nullptr, const String &content = String("")) override { server.send(code, content_type, content); }
  void sendContent(const String &content) override { server.sendContent(content); }
  void stop() override { server.client().stop(); }
};

/** HTML-escapes text sourced from runtime or remote API data. */
String htmlEscape(const String &value)
{
  String escaped;
  escaped.reserve(value.length() + 16);

  for (size_t index = 0; index < value.length(); ++index)
  {
    switch (value[index])
    {
      case '&':
        escaped += F("&amp;");
        break;
      case '<':
        escaped += F("&lt;");
        break;
      case '>':
        escaped += F("&gt;");
        break;
      case '"':
        escaped += F("&quot;");
        break;
      case '\'':
        escaped += F("&#39;");
        break;
      default:
        escaped += value[index];
        break;
    }
  }

  return escaped;
}

/** Returns escaped text or a fallback when the value is empty. */
String safeText(const String &value, const char *fallback)
{
  if (value.length() == 0)
    return htmlEscape(String(fallback));
  return htmlEscape(value);
}

/** Wraps a rendered dashboard value so long text stays visually compact. */
String dashboardClampValue(const String &renderedValue)
{
  return String(F("<span class='dashboard-clamp'>")) + renderedValue + F("</span>");
}

/** Returns true when the current request asked for a live-fragment refresh instead of a full page. */
bool isLiveRefreshRequest()
{
  return server.hasArg("partial") && server.arg("partial") == "1";
}

/** Returns true when a previously issued console page token is still valid for follow-up fetches. */
bool hasValidConsoleAccessToken()
{
  if (consoleAccessToken[0] == '\0')
    return false;
  if (!server.hasArg("token"))
    return false;
  if (strcmp(server.arg("token").c_str(), consoleAccessToken) != 0)
    return false;
  return static_cast<uint32_t>(millis() - consoleAccessTokenIssuedAt) <= kConsoleAccessTokenLifetimeMs;
}

/** Issues a fresh console page token so live fetches do not depend on browser auth quirks. */
String issueConsoleAccessToken()
{
  snprintf(consoleAccessToken, sizeof(consoleAccessToken), "%08lx%08lx",
           static_cast<unsigned long>(esp_random()),
           static_cast<unsigned long>(esp_random()));
  consoleAccessTokenIssuedAt = millis();
  return String(consoleAccessToken);
}

/** Allows console AJAX requests to use either admin auth or the current page token. */
bool authorizeConsoleRequest()
{
  if (iotWebConf.getState() != iotwebconf::OnLine)
    return true;
  if (hasValidConsoleAccessToken())
    return true;
  return authorizeAdminRequest();
}

/** Returns the address that makes sense for the current operating mode. */
String activeAddressLabel()
{
  IPAddress address = isSetupPortalState() ? WiFi.softAPIP() : WiFi.localIP();
  String rendered = address.toString();
  if (rendered == "0.0.0.0")
    return F("Unavailable");
  return htmlEscape(rendered);
}

/** Formats the current time-source tag for human-readable display. */
String timeSourceLabel()
{
  String rendered(runtimeState.timeSource);
  rendered.toLowerCase();
  if (rendered == "gps")
    rendered = F("GPS");
  else if (rendered == "ntp")
    rendered = F("NTP");
  else if (rendered == "rtc")
    rendered = F("RTC Chip");
  else
    rendered = F("None");
  return htmlEscape(rendered);
}

/** Formats the active timezone source so manual fallbacks are visible on the dashboard. */
String timezoneSourceLabel()
{
  if (enable_fixed_tz.isChecked())
    return F("Manual override");
  if (ipgeo.timezone[0] != '\0' || ipgeo.tzoffset != 127)
    return F("IP geolocation");
  if (savedtimezone.value()[0] != '\0' || savedtzoffset.value() != 0)
    return F("Saved fallback");
  if (fixed_offset.value() != 0)
    return F("Configured fallback");
  return F("GMT default");
}

/** Formats the active NTP server label shown on the dashboard and config portal. */
String ntpServerLabel()
{
  return safeText(String(runtimeState.ntpServer), DEFAULT_NTP_SERVER);
}

/** Formats the source of the active NTP server selection. */
String ntpServerSourceLabel()
{
  return safeText(String(runtimeState.ntpServerSource), "Unknown");
}

/** Returns the current temperature unit as safe inline HTML. */
String temperatureUnitHtml()
{
  return imperial.isChecked() ? String(F("&#8457;")) : String(F("&#8451;"));
}

/** Returns the current distance/speed unit label. */
String speedUnitLabel()
{
  return htmlEscape(String(imperial.isChecked() ? imperial_units[1] : metric_units[1]));
}

/** Formats a signed integer temperature with the configured unit. */
String formatTemperature(int16_t temperature)
{
  return String(temperature) + temperatureUnitHtml();
}

/** Formats the current LED brightness as a normalized 1-100 value. */
String brightnessPercentLabel()
{
  long minBrightness = 5 + runtimeState.userBrightness;
  long maxBrightness = LUXMAX + runtimeState.userBrightness;
  long percentage = map(current.brightness, minBrightness, maxBrightness, 1, 100);
  percentage = constrain(percentage, 1L, 100L);
  return String(percentage) + F("/100");
}

/** Formats a timestamp as a relative "x ago" string. */
String formatAgo(acetime_t now, acetime_t timestamp)
{
  // AceTime timestamps can be valid negative epoch values on this firmware, so
  // do not treat <= 0 as "missing". Only the library invalid sentinel, our
  // own zero-initialized "unset" fields, or future timestamps should render as
  // pending.
  if (now == LocalTime::kInvalidSeconds || timestamp == LocalTime::kInvalidSeconds || timestamp == 0 || now <= timestamp)
    return F("Pending");
  return elapsedTime(static_cast<uint32_t>(now - timestamp)) + F(" ago");
}

/** Formats a future display interval based on the last time something showed. */
String formatUntil(acetime_t now, acetime_t lastShown, uint32_t intervalSeconds)
{
  if (intervalSeconds == 0)
    return F("Ready now");

  acetime_t nextDue = lastShown + static_cast<acetime_t>(intervalSeconds);
  if (nextDue <= now)
    return F("Ready now");

  return String(F("in ")) + elapsedTime(static_cast<uint32_t>(nextDue - now));
}

/** Formats the best available location label from the current resolved location. */
String currentLocationLabel()
{
  String city;
  String state;
  String country;

  if (current.city[0] != '\0')
  {
    city = String(current.city);
    state = String(current.state);
    country = String(current.country);
  }
  else if (geocode.city[0] != '\0')
  {
    city = String(geocode.city);
    state = String(geocode.state);
    country = String(geocode.country);
  }
  else if (isLocationValid("saved"))
  {
    city = String(savedcity.value());
    state = String(savedstate.value());
    country = String(savedcountry.value());
  }

  if (city.length() == 0)
    return F("Waiting for location");

  String rendered = city;
  if (state.length() > 0)
    rendered += String(F(", ")) + state;
  if (country.length() > 0)
    rendered += String(F(", ")) + country;
  return htmlEscape(rendered);
}

/** Formats the active coordinates or an explanatory placeholder. */
String currentCoordinatesLabel()
{
  if (!isCoordsValid())
    return F("Waiting for valid coordinates");

  return htmlEscape(String(current.lat, 5) + F(", ") + String(current.lon, 5));
}

/** Formats the current weather description with a fallback while data loads. */
String currentWeatherDescription()
{
  return safeText(capString(weather.current.description), "Waiting for weather");
}

/** Formats the active alert description with a fallback to the alert title. */
String currentAlertEvent()
{
  const AlertEntry *entry = primaryAlert();
  if (entry == nullptr)
    return F("No active alert");
  if (hasVisibleText(entry->event))
    return htmlEscape(String(entry->event));
  if (hasVisibleText(entry->headline))
    return htmlEscape(String(entry->headline));
  return F("Unnamed alert");
}

/** Formats the active alert description with a fallback to the alert title. */
String currentAlertDescription()
{
  const AlertEntry *entry = primaryAlert();
  if (entry == nullptr)
    return F("No active alert");
  if (hasVisibleText(entry->description))
    return htmlEscape(String(entry->description));
  if (hasVisibleText(entry->headline))
    return htmlEscape(String(entry->headline));
  if (hasVisibleText(entry->event))
    return htmlEscape(String(entry->event));
  return F("No active alert");
}

/** Formats the condensed alert text currently used by the matrix scroller. */
String currentAlertDisplayText()
{
  const AlertEntry *entry = primaryAlert();
  if (entry == nullptr)
    return F("No active alert");
  if (hasVisibleText(entry->displayText))
    return htmlEscape(String(entry->displayText));
  if (hasVisibleText(entry->event))
    return htmlEscape(String(entry->event));
  return F("No active alert");
}

/** Summarizes the active alerts in priority order for the dashboard. */
String activeAlertsLabel()
{
  return htmlEscape(summarizeActiveAlerts());
}

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
}

/** Resets the per-request JSON config-import state tracked by the backup page. */
void resetConfigImportState()
{
  configImportAuthenticated = false;
  configImportStarted = false;
  configImportError = "";
  configImportPayload = "";
}

/** Formats the current AQI description with a fallback based on the AQI bucket. */
String currentAqiDescription()
{
  if (aqi.current.description.length() > 0)
    return htmlEscape(aqi.current.description);
  return htmlEscape(String(air_quality[aqi.current.aqi]));
}

/** Selects a tone class that visually summarizes the current AQI bucket. */
const char *aqiTone()
{
  switch (aqi.current.aqi)
  {
    case 1:
      return "tone-good";
    case 2:
    case 3:
      return "tone-warn";
    case 4:
    case 5:
      return "tone-bad";
    default:
      return "tone-neutral";
  }
}

/** Selects a tone class that visually summarizes the alert situation. */
const char *alertTone()
{
  if (alerts.warningCount > 0)
    return "tone-bad";
  if (alerts.active || alerts.watchCount > 0)
    return "tone-warn";
  return "tone-good";
}

/** Returns the current diagnostics record for the selected subsystem. */
const ServiceDiagnostic &serviceDiagnostic(DiagnosticService service)
{
  return diagnosticState(service);
}

/** Returns the diagnostics card tone for the selected subsystem. */
const char *diagnosticTone(DiagnosticService service)
{
  const ServiceDiagnostic &diag = serviceDiagnostic(service);
  if (!diag.enabled)
    return "tone-neutral";
  if (diag.healthy)
    return "tone-good";
  if (diag.lastFailure > 0)
    return "tone-bad";
  return "tone-warn";
}

/** Returns a short diagnostics status label with disabled fallback handling. */
String diagnosticSummary(DiagnosticService service)
{
  const ServiceDiagnostic &diag = serviceDiagnostic(service);
  if (!diag.enabled)
    return F("Disabled");
  return safeText(String(diag.summary), "Pending");
}

/** Returns the longer diagnostics detail text for one subsystem. */
String diagnosticDetail(DiagnosticService service)
{
  const ServiceDiagnostic &diag = serviceDiagnostic(service);
  if (!diag.enabled)
    return F("Disabled by current configuration.");
  return safeText(String(diag.detail), "Waiting for runtime data.");
}

/** Formats the last recorded error or HTTP code for one subsystem. */
String diagnosticCodeLabel(DiagnosticService service)
{
  const ServiceDiagnostic &diag = serviceDiagnostic(service);
  if (diag.lastCode == 0)
    return F("None");

  if (diag.lastCode > 0)
    return htmlEscape(String(F("HTTP ")) + diag.lastCode + F(" ") + getHttpCodeName(diag.lastCode));

  return htmlEscape(String(diag.lastCode) + F(" ") + getHttpCodeName(diag.lastCode));
}

/** Counts diagnostics records by the requested health bucket. */
size_t countDiagnostics(bool enabled, bool healthy)
{
  size_t count = 0;
  for (size_t index = 0; index < kDiagnosticServiceCount; ++index)
  {
    const ServiceDiagnostic &diag = runtimeState.diagnostics[index];
    if (diag.enabled == enabled && diag.healthy == healthy)
      ++count;
  }
  return count;
}

/** Counts enabled services that are still waiting rather than actively failing. */
size_t countPendingDiagnostics()
{
  size_t count = 0;
  for (size_t index = 0; index < kDiagnosticServiceCount; ++index)
  {
    const ServiceDiagnostic &diag = runtimeState.diagnostics[index];
    if (diag.enabled && !diag.healthy && diag.lastFailure == 0)
      ++count;
  }
  return count;
}

/** Counts enabled services that have a recorded failure and need attention. */
size_t countAttentionDiagnostics()
{
  size_t count = 0;
  for (size_t index = 0; index < kDiagnosticServiceCount; ++index)
  {
    const ServiceDiagnostic &diag = runtimeState.diagnostics[index];
    if (diag.enabled && !diag.healthy && diag.lastFailure > 0)
      ++count;
  }
  return count;
}

/** Counts services that are intentionally disabled by config or setup state. */
size_t countDisabledDiagnostics()
{
  size_t count = 0;
  for (size_t index = 0; index < kDiagnosticServiceCount; ++index)
  {
    if (!runtimeState.diagnostics[index].enabled)
      ++count;
  }
  return count;
}

/** Returns a short dashboard badge describing the current diagnostics state. */
String diagnosticHealthBadge(DiagnosticService service)
{
  const ServiceDiagnostic &diag = serviceDiagnostic(service);
  if (!diag.enabled)
    return F("Disabled");
  if (diag.healthy)
    return F("Healthy");
  if (diag.lastFailure > 0)
    return F("Attention");
  return F("Waiting");
}

/** Returns the most relevant recent timestamp for the diagnostics summary card. */
String diagnosticRecentActivity(DiagnosticService service, acetime_t now)
{
  const ServiceDiagnostic &diag = serviceDiagnostic(service);
  if (!diag.enabled)
    return F("Disabled");
  if (diag.healthy)
    return formatAgo(now, diag.lastSuccess);
  if (diag.lastFailure > 0)
    return formatAgo(now, diag.lastFailure);
  return F("Waiting");
}

/** Renders one compact service-health card on the dashboard. */
void appendServiceHealthCard(String &html, DiagnosticService service, acetime_t now)
{
  html += F("<article class='service-health-card ");
  html += diagnosticTone(service);
  html += F("'><div class='service-health-card-head'><h3>");
  html += htmlEscape(String(diagnosticServiceLabel(service)));
  html += F("</h3><span>");
  html += htmlEscape(diagnosticSummary(service));
  html += F("</span></div><p>");
  html += htmlEscape(diagnosticDetail(service));
  html += F("</p><div class='service-health-meta'><span>");
  html += htmlEscape(diagnosticHealthBadge(service));
  html += F("</span><strong>");
  html += htmlEscape(diagnosticRecentActivity(service, now));
  html += F("</strong></div></article>");
}

/** Opens the page shell and injects the shared CSS theme. */
void appendDocumentHead(String &html, const char *title, bool autoRefresh)
{
  const bool darkMode = web_dark_mode.isChecked();
  html += F("<!DOCTYPE html><html lang='en' data-web-theme='");
  html += (darkMode ? F("dark") : F("light"));
  html += F("'><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'>");
  html += F("<meta name='theme-color' content='");
  html += (darkMode ? F("#101d28") : F("#173748"));
  html += F("'>");
  if (autoRefresh)
    html += F("<noscript><meta http-equiv='refresh' content='15'></noscript>");
  html += F("<title>");
  html += htmlEscape(String(title));
  html += F("</title><link rel='stylesheet' href='");
  html += kThemeCssPath;
  html += F("?v=");
  html += VERSION_SEMVER;
  html += F("'></head><body>");
}

/** Opens the live-refresh shell used by pages that update in place. */
void appendLiveShellStart(String &html, const char *refreshPath)
{
  html += F("<div id='live-shell' class='shell' data-refresh-path='");
  html += refreshPath;
  html += F("' data-refresh-interval='");
  html += String(kLiveRefreshIntervalMs);
  html += F("'>");
}

/** Closes the live-refresh shell wrapper. */
void appendLiveShellEnd(String &html)
{
  html += F("</div>");
}

/** Adds a pill-style status chip to the current page. */
void appendStatusChip(String &html, const char *label, const String &value)
{
  html += F("<div class='status-chip'><strong>");
  html += htmlEscape(String(label));
  html += F("</strong><span>");
  html += value;
  html += F("</span></div>");
}

/** Adds a large summary metric card to the current page. */
void appendMetricCard(String &html, const char *label, const String &value, const char *toneClass)
{
  html += F("<article class='metric-card ");
  html += toneClass;
  html += F("'><span>");
  html += htmlEscape(String(label));
  html += F("</span><strong>");
  html += value;
  html += F("</strong></article>");
}

/** Opens a detail card with a title and supporting subtitle. */
void appendCardStart(String &html, const char *title, const char *subtitle)
{
  html += F("<section class='card'><div class='card-header'><h2>");
  html += htmlEscape(String(title));
  html += F("</h2><p class='card-subtitle'>");
  html += htmlEscape(String(subtitle));
  html += F("</p></div><dl class='kv-list'>");
}

/** Opens a detail card with a visual tone summary. */
void appendTonedCardStart(String &html, const char *title, const char *subtitle, const char *toneClass)
{
  html += F("<section class='card ");
  html += toneClass;
  html += F("'><div class='card-header'><h2>");
  html += htmlEscape(String(title));
  html += F("</h2><p class='card-subtitle'>");
  html += htmlEscape(String(subtitle));
  html += F("</p></div><dl class='kv-list'>");
}

/** Adds a key/value row to the currently open detail card. */
void appendKeyValueRow(String &html, const char *label, const String &value)
{
  html += F("<div class='kv-row'><dt>");
  html += htmlEscape(String(label));
  html += F("</dt><dd>");
  html += value;
  html += F("</dd></div>");
}

/** Closes a previously opened detail card. */
void appendCardEnd(String &html)
{
  html += F("</dl></section>");
}

/** Adds a highlighted notice when attention is required on the landing page. */
void appendNotice(String &html, const char *toneClass, const String &message)
{
  html += F("<section class='notice ");
  html += toneClass;
  html += F("'>");
  html += message;
  html += F("</section>");
}

/** Builds the default onboarding draft from the current in-memory configuration values. */
OnboardingDraft currentOnboardingDraft()
{
  OnboardingDraft draft;
  draft.wifiSsid = String(iotWebConf.getWifiSsidParameter()->valueBuffer);
  draft.wifiPassword = String(iotWebConf.getWifiPasswordParameter()->valueBuffer);
  draft.enableWebPassword = web_password_protection.isChecked();
  draft.adminPassword = draft.enableWebPassword ? String(iotWebConf.getApPasswordParameter()->valueBuffer) : String();
  draft.weatherApiKey = String(weatherapi.value());
  draft.ipGeoApiKey = String(ipgeoapi.value());
  draft.useFixedTimezone = enable_fixed_tz.isChecked();
  draft.fixedOffset = fixed_offset.value();
  return draft;
}

/** Copies a classic IotWebConf parameter buffer with bounds checking. */
bool copyParameterValue(iotwebconf::Parameter &parameter, const String &value, const char *fieldLabel, String &error)
{
  if (value.length() + 1 > static_cast<size_t>(parameter.getLength()))
  {
    error = String(fieldLabel) + F(" is too long for this device.");
    return false;
  }

  strlcpy(parameter.valueBuffer, value.c_str(), static_cast<size_t>(parameter.getLength()));
  return true;
}

/** Copies a typed char-array parameter with bounds checking. */
template <size_t N>
bool copyParameterValue(iotwebconf::TextTParameter<N> &parameter, const String &value, const char *fieldLabel, String &error)
{
  if (value.length() + 1 > N)
  {
    error = String(fieldLabel) + F(" is too long for this device.");
    return false;
  }

  strlcpy(parameter.value(), value.c_str(), N);
  return true;
}

/** Builds an onboarding draft from the current POST body so invalid submissions can be re-rendered. */
OnboardingDraft submittedOnboardingDraft()
{
  OnboardingDraft draft = currentOnboardingDraft();
  draft.wifiSsid = server.hasArg("wifi_ssid") ? server.arg("wifi_ssid") : draft.wifiSsid;
  draft.wifiPassword = server.hasArg("wifi_password") ? server.arg("wifi_password") : draft.wifiPassword;
  draft.enableWebPassword = !server.hasArg("web_protection") || server.arg("web_protection") != "disabled";
  draft.adminPassword = server.hasArg("admin_password") ? server.arg("admin_password") : draft.adminPassword;
  draft.weatherApiKey = server.hasArg("weather_api") ? server.arg("weather_api") : draft.weatherApiKey;
  draft.ipGeoApiKey = server.hasArg("ipgeo_api") ? server.arg("ipgeo_api") : draft.ipGeoApiKey;
  draft.useFixedTimezone = server.hasArg("timezone_mode") && server.arg("timezone_mode") == "fixed";
  if (server.hasArg("fixed_offset"))
    draft.fixedOffset = static_cast<int8_t>(constrain(server.arg("fixed_offset").toInt(), -12, 12));
  return draft;
}

/** Renders one consistent onboarding or login form field. */
void appendFormField(String &html, const char *label, const char *name, const String &value,
                     const char *type, const char *help = nullptr, const char *extraAttributes = nullptr)
{
  html += F("<div class='form-field'><label for='");
  html += name;
  html += F("'>");
  html += htmlEscape(String(label));
  html += F("</label><input id='");
  html += name;
  html += F("' name='");
  html += name;
  html += F("' type='");
  html += type;
  html += F("' value='");
  html += htmlEscape(value);
  html += F("'");
  if (extraAttributes != nullptr && extraAttributes[0] != '\0')
  {
    html += ' ';
    html += extraAttributes;
  }
  html += F(">");
  if (help != nullptr && help[0] != '\0')
  {
    html += F("<p class='field-help'>");
    html += htmlEscape(String(help));
    html += F("</p>");
  }
  html += F("</div>");
}

/** Renders the physical password recovery instructions shared by onboarding and login. */
void appendPasswordRecoveryPanel(String &html)
{
  html += F("<section class='recovery-panel'><h2>Password Recovery</h2><p>If you forget the web password, hold the configuration button on GPIO ");
  html += String(CONFIG_PIN);
  html += F(" while powering on or resetting the clock. The device will return to the recovery setup portal so you can set a new password. In recovery mode the setup access point uses the default password <code>");
  html += htmlEscape(String(wifiInitialApPassword));
  html += F("</code>.</p></section>");
}

/** Renders one compact self-test result card used at the end of onboarding. */
void appendSelfTestItem(String &html, const char *title, const char *toneClass, const String &summary, const String &detail)
{
  html += F("<article class='self-test-item ");
  html += toneClass;
  html += F("'><h3>");
  html += htmlEscape(String(title));
  html += F("</h3><p><strong>");
  html += htmlEscape(summary);
  html += F("</strong><br>");
  html += htmlEscape(detail);
  html += F("</p></article>");
}

/** Renders the live hardware and service checks shown after onboarding saves. */
void appendOnboardingSelfTest(String &html)
{
  const bool rtcReady = systemClock.isInit();
  const bool lightReady = Tsl.available();
  const bool gpsReady = gps.moduleDetected || GPS.charsProcessed() > 0;
  const bool networkReady = iotWebConf.getState() == iotwebconf::OnLine;

  html += F("<section class='card'><div class='card-header'><h2>Final Self-Test</h2><p class='card-subtitle'>Use this quick checklist to confirm the new setup is healthy. If Wi-Fi is still in captive-portal mode, disconnect from the setup network so the clock can join your configured SSID and finish the remaining checks.</p></div><div class='self-test-grid'>");
  appendSelfTestItem(html, "RTC Chip", rtcReady ? "tone-good" : "tone-bad",
                     rtcReady ? String(F("Detected")) : String(F("Unavailable")),
                     rtcReady ? String(F("The DS3231 clock responded during startup.")) : String(F("The RTC did not report a valid startup time.")));
  appendSelfTestItem(html, "Light Sensor", lightReady ? "tone-good" : "tone-warn",
                     lightReady ? String(F("Detected")) : String(F("Waiting")),
                     lightReady ? String(F("Ambient brightness control is available.")) : String(F("The TSL2561 sensor has not reported ready yet.")));
  appendSelfTestItem(html, "GPS Receiver", gpsReady ? "tone-good" : "tone-warn",
                     gpsReady ? String(F("UART active")) : String(F("No UART traffic yet")),
                     gpsReady ? String(F("The GPS parser is receiving data from the module.")) : String(F("Keep the GPS powered and check for satellite traffic after setup.")));
  appendSelfTestItem(html, "Network", networkReady ? "tone-good" : "tone-warn",
                     networkReady ? String(F("Connected")) : String(F("Waiting for Wi-Fi")),
                     networkReady ? String(F("The clock is online at ")) + WiFi.localIP().toString()
                                  : String(F("Disconnect from the setup AP so the clock can join the configured Wi-Fi network.")));
  appendSelfTestItem(html, "Weather APIs", isApiValid(weatherapi.value()) ? "tone-good" : "tone-warn",
                     isApiValid(weatherapi.value()) ? String(F("Configured")) : String(F("Missing OpenWeather key")),
                     isApiValid(weatherapi.value()) ? String(F("Weather, forecast, reverse geocode, and AQI requests can run once networking is ready."))
                                                    : String(F("Add the OpenWeather key in onboarding or advanced configuration.")));
  appendSelfTestItem(html, "Timezone", enable_fixed_tz.isChecked() ? "tone-good" : "tone-neutral",
                     enable_fixed_tz.isChecked() ? String(F("Fixed GMT offset")) : String(F("Automatic timezone")),
                     enable_fixed_tz.isChecked() ? String(F("Using GMT ")) + String(fixed_offset.value())
                                                 : String(F("The clock will use geolocation or saved timezone data for DST-aware local time.")));
  html += F("</div><div class='action-row'>");
  html += F("<a class='button-link secondary' href='");
  html += kOnboardingMatrixTestPath;
  html += F("?return=");
  html += urlEncode(String(kOnboardingPath) + F("?saved=1"));
  html += F("'>Run Matrix Test</a>");
  html += F("<a class='button-link ghost' href='/'>Continue to the Clock</a>");
  html += F("<a class='button-link ghost' href='/config'>Open Advanced Configuration</a>");
  html += F("</div></section>");
}

/** Renders the password-only login page that protects the normal web UI. */
void appendLoginPage(String &html, const char *noticeToneClass, const String &noticeMessage, const String &nextPath)
{
  appendDocumentHead(html, "LED Smart Clock Login", false);

  html += F("<div class='auth-shell'><header class='hero'>");
  html += F("<p class='eyebrow'>LED Smart Clock Secure Access</p>");
  html += F("<h1>Enter Password</h1>");
  html += F("<p class='lede'>The dashboard, diagnostics, console, firmware tools, and configuration portal all use the password set for this clock. Enter it once to start a secure browser session.</p>");
  html += F("</header>");

  if (noticeToneClass != nullptr && noticeMessage.length() > 0)
    appendNotice(html, noticeToneClass, noticeMessage);

  html += F("<section class='auth-card'><div class='card-header'><h2>Web Login</h2><p class='card-subtitle'>This login only asks for the password. The username is fixed internally and is not shown on the custom web interface.</p></div>");
  html += F("<form class='auth-form' method='POST' action='");
  html += kLoginPath;
  html += F("'><input type='hidden' name='next' value='");
  html += htmlEscape(nextPath);
  html += F("'><div class='field-grid'>");
  appendFormField(html, "Password", "password", String(), "password",
                  "This is the same password used to secure the setup portal and the normal web interface.",
                  "autocomplete='current-password' required");
  html += F("</div><div class='onboarding-actions'><span class='spacer'></span><button type='submit'>Sign In</button></div></form></section>");

  appendPasswordRecoveryPanel(html);
  html += F("</div></body></html>");
}

/** Renders the first-boot onboarding flow used in AP or recovery mode. */
void appendSetupPage(String &html, const OnboardingDraft *draft, const char *noticeToneClass, const String &noticeMessage, bool showSelfTest)
{
  OnboardingDraft effectiveDraft = draft != nullptr ? *draft : currentOnboardingDraft();
  appendDocumentHead(html, "LED Smart Clock Setup", false);

  html += F("<div class='shell'><header class='hero'>");
  html += F("<p class='eyebrow'>LED Smart Clock</p>");
  html += F("<h1>First-Boot Onboarding</h1>");
  html += F("<p class='lede'>Walk through Wi-Fi, optional web password protection, API keys, timezone behavior, and a final self-test for firmware v");
  html += VERSION_SEMVER;
  html += F(".</p><div class='action-row'>");
  html += F("<a class='button-link primary' href='");
  html += kConfigImportPath;
  html += F("'>Import Existing Backup</a>");
  html += F("<a class='button-link secondary' href='/config'>Advanced Configuration</a>");
  html += F("<a class='button-link secondary' href='");
  html += kDiagnosticsPath;
  html += F("'>Diagnostics</a>");
  html += F("</div><div class='portal-chip-row'>");
  appendStatusChip(html, "State", htmlEscape(currentConnectionStateLabel()));
  appendStatusChip(html, "Access Point", htmlEscape(String(thingName)));
  appendStatusChip(html, "Portal Address", activeAddressLabel());
  html += F("</div><div class='metric-grid'>");
  appendMetricCard(html, "Firmware", htmlEscape(String(VERSION_SEMVER)), "tone-neutral");
  appendMetricCard(html, "Wi-Fi Mode", F("Captive Portal"), "tone-warn");
  appendMetricCard(html, "Web Access", effectiveDraft.enableWebPassword ? F("Protected") : F("Open"), effectiveDraft.enableWebPassword ? "tone-good" : "tone-neutral");
  appendMetricCard(html, "Weather API", isApiValid(weatherapi.value()) ? F("Configured") : F("Optional"), isApiValid(weatherapi.value()) ? "tone-good" : "tone-warn");
  html += F("</div></header>");

  appendNotice(html, "notice-good", F("Already have a backup from another clock or older firmware? Import it first so Wi-Fi credentials, passwords, API keys, and display preferences all restore in one step."));
  if (noticeToneClass != nullptr && noticeMessage.length() > 0)
    appendNotice(html, noticeToneClass, noticeMessage);

  html += F("<section class='card'><div class='card-header'><h2>Guided Setup</h2><p class='card-subtitle'>Without JavaScript every section stays visible. With JavaScript enabled, the Next and Back buttons turn this into a focused step-by-step setup flow.</p></div><div class='wizard-progress'>");
  html += F("<span class='step-pill' data-step-pill='1'><strong>1</strong>Wi-Fi & Access</span>");
  html += F("<span class='step-pill' data-step-pill='2'><strong>2</strong>API Keys</span>");
  html += F("<span class='step-pill' data-step-pill='3'><strong>3</strong>Timezone</span>");
  html += F("<span class='step-pill' data-step-pill='4'><strong>4</strong>Review & Save</span></div>");
  html += F("<form class='onboarding-form' method='POST' action='");
  html += kOnboardingPath;
  html += F("'>");

  html += F("<section class='onboarding-step' data-step='1'><div class='card-header'><h2>Step 1: Wi-Fi & Access</h2><p class='card-subtitle'>Tell the clock which Wi-Fi network to join, then choose whether the dashboard and maintenance pages should require a password after onboarding is finished.</p></div><div class='field-grid'>");
  appendFormField(html, "Wi-Fi SSID", "wifi_ssid", effectiveDraft.wifiSsid, "text",
                  "This is the normal Wi-Fi network the clock should join after setup.", "autocomplete='username' required");
  appendFormField(html, "Wi-Fi Password", "wifi_password", effectiveDraft.wifiPassword, "password",
                  "Leave blank only for an open Wi-Fi network.", "autocomplete='current-password'");
  html += F("<div class='form-field'><label for='web_protection'>Password Protect The Web Interface</label><select id='web_protection' name='web_protection'><option value='enabled'");
  if (effectiveDraft.enableWebPassword)
    html += F(" selected");
  html += F(">Enabled (recommended)</option><option value='disabled'");
  if (!effectiveDraft.enableWebPassword)
    html += F(" selected");
  html += F(">Disabled</option></select><p class='field-help'>When enabled, the dashboard, diagnostics, console, firmware page, and configuration portal require the clock password before access.</p></div>");
  appendFormField(html, "Clock Web Password", "admin_password", effectiveDraft.adminPassword, "password",
                  "Only required when password protection is enabled above. Recovery setup still remains available with the physical configuration button.",
                  "autocomplete='new-password' minlength='8'");
  html += F("</div><div class='onboarding-actions'><span class='spacer'></span><button type='button' data-next-step='2'>Next: API Keys</button></div></section>");

  html += F("<section class='onboarding-step' data-step='2'><div class='card-header'><h2>Step 2: API Keys</h2><p class='card-subtitle'>Add the service keys the clock uses for weather, air quality, reverse geocoding, and automatic timezone/location fallback.</p></div><div class='field-grid'>");
  appendFormField(html, "OpenWeather API Key", "weather_api", effectiveDraft.weatherApiKey, "text",
                  "Recommended. Used for current weather, forecast, AQI, and reverse geocoding.", "autocomplete='off'");
  appendFormField(html, "IPGeolocation.io API Key", "ipgeo_api", effectiveDraft.ipGeoApiKey, "text",
                  "Optional but recommended for automatic timezone and coarse IP location fallback.", "autocomplete='off'");
  html += F("</div><p class='field-help'>If you are restoring from a backup, these keys are usually already included there.</p><div class='onboarding-actions'><button type='button' class='button-link ghost' data-prev-step='1'>Back</button><span class='spacer'></span><button type='button' data-next-step='3'>Next: Timezone</button></div></section>");

  html += F("<section class='onboarding-step' data-step='3'><div class='card-header'><h2>Step 3: Timezone Behavior</h2><p class='card-subtitle'>Choose whether the clock should determine local time automatically or always use a fixed GMT offset.</p></div><div class='field-grid'><div class='form-field'><label for='timezone_mode'>Timezone Mode</label><select id='timezone_mode' name='timezone_mode'><option value='auto'");
  if (!effectiveDraft.useFixedTimezone)
    html += F(" selected");
  html += F(">Automatic (recommended)</option><option value='fixed'");
  if (effectiveDraft.useFixedTimezone)
    html += F(" selected");
  html += F(">Fixed GMT offset</option></select><p class='field-help'>Automatic mode allows DST-aware timezone handling when the clock knows its location or saved timezone.</p></div>");
  appendFormField(html, "Fixed GMT Offset", "fixed_offset", String(effectiveDraft.fixedOffset), "number",
                  "Only used when Fixed GMT offset mode is selected above.", "min='-12' max='12' step='1'");
  html += F("</div><div class='onboarding-actions'><button type='button' class='button-link ghost' data-prev-step='2'>Back</button><span class='spacer'></span><button type='button' data-next-step='4'>Next: Review</button></div></section>");

  html += F("<section class='onboarding-step' data-step='4'><div class='card-header'><h2>Step 4: Review & Save</h2><p class='card-subtitle'>Save the onboarding values into the clock, then use the self-test below to verify hardware and service readiness.</p></div><div class='self-test-grid'>");
  appendSelfTestItem(html, "Wi-Fi Ready", effectiveDraft.wifiSsid.length() > 0 ? "tone-good" : "tone-bad",
                     effectiveDraft.wifiSsid.length() > 0 ? String(F("SSID entered")) : String(F("Missing SSID")),
                     effectiveDraft.wifiSsid.length() > 0 ? String(F("The clock can attempt to join ")) + effectiveDraft.wifiSsid
                                                          : String(F("Enter the Wi-Fi network name before saving onboarding.")));
  appendSelfTestItem(html, "Web Access", effectiveDraft.enableWebPassword
                                                ? (effectiveDraft.adminPassword.length() >= 8 ? "tone-good" : "tone-bad")
                                                : "tone-neutral",
                     effectiveDraft.enableWebPassword
                         ? (effectiveDraft.adminPassword.length() >= 8 ? String(F("Password protection enabled")) : String(F("Password too short")))
                         : String(F("No password required")),
                     effectiveDraft.enableWebPassword
                         ? (effectiveDraft.adminPassword.length() >= 8 ? String(F("The web interface will require the clock password after setup."))
                                                                      : String(F("Use at least 8 characters or disable protection before saving.")))
                         : String(F("The dashboard and maintenance pages will open directly without a login.")));
  appendSelfTestItem(html, "OpenWeather", effectiveDraft.weatherApiKey.length() > 0 ? "tone-good" : "tone-warn",
                     effectiveDraft.weatherApiKey.length() > 0 ? String(F("Configured")) : String(F("Optional but missing")),
                     effectiveDraft.weatherApiKey.length() > 0 ? String(F("Weather, AQI, and reverse-geocode services can run."))
                                                               : String(F("You can still finish setup now and add the key later.")));
  appendSelfTestItem(html, "Timezone", effectiveDraft.useFixedTimezone ? "tone-good" : "tone-neutral",
                     effectiveDraft.useFixedTimezone ? String(F("Fixed GMT offset")) : String(F("Automatic")),
                     effectiveDraft.useFixedTimezone ? String(F("The clock will always use GMT ")) + String(effectiveDraft.fixedOffset)
                                                     : String(F("The clock will use automatic location and timezone data when available.")));
  html += F("</div><div class='onboarding-actions'><button type='button' class='button-link ghost' data-prev-step='3'>Back</button><span class='spacer'></span><button type='submit'>Save Onboarding</button></div></section></form></section>");

  if (showSelfTest)
    appendOnboardingSelfTest(html);

  appendPasswordRecoveryPanel(html);
  html += F("<script>");
  html += FPSTR(kOnboardingScript);
  html += F("</script></div></body></html>");
}

/** Renders the themed firmware update page and optional status notice. */
void appendFirmwareUpdatePage(String &html, const char *noticeToneClass, const String &noticeMessage, bool showForm)
{
  appendDocumentHead(html, "LED Smart Clock Firmware Update", false);

  html += F("<div class='shell'><header class='hero'>");
  html += F("<p class='eyebrow'>LED Smart Clock Maintenance</p>");
  html += F("<h1>Firmware Update</h1>");
  html += F("<p class='lede'>Upload the compiled <code>update.bin</code> OTA image to install a new release over the network without opening the enclosure. For a first-time install on a blank ESP32, use the web installer and release <code>firmware.bin</code> image instead.</p><div class='action-row'>");
  html += F("<a class='button-link primary' href='/'>Dashboard</a>");
  html += F("<a class='button-link secondary' href='/config'>Configuration</a>");
  html += F("<a class='button-link secondary' href='");
  html += kDiagnosticsPath;
  html += F("'>Diagnostics</a>");
  html += F("<a class='button-link secondary' href='");
  html += kConsolePath;
  html += F("'>Console</a>");
  html += F("<a class='button-link ghost' href='");
  html += kLogoutPath;
  html += F("'>Log Out</a>");
  html += F("</div><div class='portal-chip-row'>");
  appendStatusChip(html, "Firmware", htmlEscape(String(VERSION_SEMVER)));
  appendStatusChip(html, "Address", activeAddressLabel());
  appendStatusChip(html, "Update Path", firmwareUpdatePathLabel());
  html += F("</div><div class='metric-grid'>");
  appendMetricCard(html, "Accepted File", F("update.bin"), "tone-neutral");
  appendMetricCard(html, "Expected Size", F("About 2 MB"), "tone-neutral");
  appendMetricCard(html, "Upload Type", F("Application OTA"), "tone-good");
  html += F("</div></header>");

  if (noticeToneClass != nullptr && noticeMessage.length() > 0)
    appendNotice(html, noticeToneClass, noticeMessage);

  html += F("<main class='content-grid'>");

  appendCardStart(html, "Back Up Configuration First", "Firmware updates are safer when you keep a current JSON backup of the clock settings before flashing.");
  appendKeyValueRow(html, "Recommended Action", String(F("<a class='button-link primary' href='")) + kConfigExportPath + F("'>Download Config Backup</a>"));
  appendKeyValueRow(html, "What It Includes", F("Wi-Fi credentials, portal password, API keys, display preferences, location overrides, and saved fallback state"));
  appendKeyValueRow(html, "Restore Path", String(F("<a class='button-link ghost' href='")) + kConfigImportPath + F("'>Open Backup & Restore</a>"));
  appendCardEnd(html);

  appendCardStart(html, "Before You Upload", "Use the OTA application image only. This screen does not replace the bootloader or partition table.");
  appendKeyValueRow(html, "Required File", F("<code>update.bin</code>"));
  appendKeyValueRow(html, "Typical Location", F("<code>.pio/build/esp32dev/update.bin</code>"));
  appendKeyValueRow(html, "Release Download", F("Download <code>firmware.zip</code> from GitHub Releases, then extract and upload <code>update.bin</code>"));
  appendKeyValueRow(html, "First Install", F("Use the online installer with release <code>firmware.bin</code> instead of this OTA page"));
  appendKeyValueRow(html, "Typical Size", F("Approximately 2 MB"));
  appendKeyValueRow(html, "Do Not Upload", F("<code>bootloader.bin</code> or <code>partitions.bin</code>"));
  appendCardEnd(html);

  html += F("<section class='card upload-panel'><div class='card-header'><h2>Upload Firmware</h2><p class='card-subtitle'>Choose the freshly built <code>update.bin</code> file, then submit it once. The device will validate, write, and reboot into the new firmware if the upload succeeds.</p></div>");

  if (showForm)
  {
    html += F("<form class='upload-form' method='POST' action='");
    html += kFirmwareUpdatePath;
    html += F("' enctype='multipart/form-data'>");
    html += F("<section class='upload-field'><h3>Select Firmware Image</h3><p>Only the OTA application image is supported here. Leave the full first-install <code>firmware.bin</code> image and any compressed packages for separate workflows.</p><input id='firmware-upload' type='file' name='firmware' accept='.bin' required></section>");
    html += F("<div class='upload-actions'><button type='submit'>Upload Firmware</button><a class='button-link ghost' href='/'>Cancel</a></div></form>");
    html += F("<div id='upload-popup' class='upload-popup' hidden><section id='upload-popup-card' class='upload-popup-card tone-neutral' aria-live='polite'><h3 id='upload-popup-title'>Ready to upload</h3><p id='upload-popup-text'>Choose the compiled <code>update.bin</code> image to begin.</p><div id='upload-progress' class='upload-progress' hidden><div class='upload-progress-track'><span id='upload-progress-bar' class='upload-progress-bar'></span></div><div id='upload-progress-label' class='upload-progress-label'>0% transferred</div><div id='upload-progress-meta' class='upload-progress-meta'>The browser will show transfer progress here while the OTA image uploads.</div></div><button id='upload-dismiss' class='upload-dismiss' type='button' hidden>Dismiss</button></section></div>");
  }

  html += F("<ul class='upload-list'>");
  html += F("<li>Download a JSON configuration backup before installing a new firmware so Wi-Fi, API keys, and display settings can be restored quickly if needed.</li>");
  html += F("<li>Get updates from the project GitHub Releases page. If you download <code>firmware.zip</code>, extract it first and upload only <code>update.bin</code> here.</li>");
  html += F("<li>Use the release <code>firmware.bin</code> image only for first-time installs through the web installer or a full USB flash workflow.</li>");
  html += F("<li>Upload the same <code>update.bin</code> you would normally flash to the OTA app partition.</li>");
  html += F("<li>Keep the browser open until the transfer completes and the success message appears.</li>");
  html += F("<li>If a release changes <code>bootloader.bin</code> or <code>partitions.bin</code>, install those over USB instead of this page.</li>");
  html += F("</ul></section></main>");
  if (showForm)
  {
    html += F("<script>");
    html += FPSTR(kFirmwareUploadScript);
    html += F("</script>");
  }
  html += F("</div></body></html>");
}

/** Renders the configuration backup and restore page used by setup and maintenance flows. */
void appendConfigBackupPage(String &html, const char *noticeToneClass, const String &noticeMessage, bool showForm)
{
  appendDocumentHead(html, "LED Smart Clock Backup & Restore", false);

  html += F("<div class='shell'><header class='hero'>");
  html += F("<p class='eyebrow'>LED Smart Clock Maintenance</p>");
  html += F("<h1>Backup & Restore</h1>");
  html += F("<p class='lede'>Download a JSON snapshot of the current configuration or restore one from another clock or older firmware. Import matches fields by key so older backups can seed newer builds without depending on storage order.</p><div class='action-row'>");
  html += F("<a class='button-link primary' href='/config'>Configuration</a>");
  html += F("<a class='button-link secondary' href='/'>Dashboard</a>");
  html += F("<a class='button-link secondary' href='");
  html += kDiagnosticsPath;
  html += F("'>Diagnostics</a>");
  html += F("<a class='button-link secondary' href='");
  html += kConsolePath;
  html += F("'>Console</a>");
  html += F("<a class='button-link ghost' href='");
  html += kLogoutPath;
  html += F("'>Log Out</a>");
  html += F("</div><div class='portal-chip-row'>");
  appendStatusChip(html, "Firmware", htmlEscape(String(VERSION_SEMVER)));
  appendStatusChip(html, "Address", activeAddressLabel());
  html += F("</div><div class='metric-grid'>");
  appendMetricCard(html, "Backup Format", F("JSON"), "tone-neutral");
  appendMetricCard(html, "Migration Style", F("Key-Based"), "tone-good");
  appendMetricCard(html, "Includes Secrets", F("Yes"), "tone-warn");
  html += F("</div></header>");

  if (noticeToneClass != nullptr && noticeMessage.length() > 0)
    appendNotice(html, noticeToneClass, noticeMessage);

  html += F("<main class='content-grid'>");

  appendCardStart(html, "Export Configuration", "Download a backup before firmware changes, resets, or experimenting with settings so the full setup can be restored quickly.");
  appendKeyValueRow(html, "Download", String(F("<a class='button-link primary' href='")) + kConfigExportPath + F("'>Download Config Backup</a>"));
  appendKeyValueRow(html, "Filename", htmlEscape(configBackupDownloadFileName()));
  appendKeyValueRow(html, "Contains", F("Wi-Fi credentials, portal password, API keys, display settings, location overrides, and saved fallback values"));
  appendKeyValueRow(html, "Handle Carefully", F("The backup includes secrets, so store it privately."));
  appendCardEnd(html);

  html += F("<section class='card upload-panel'><div class='card-header'><h2>Import Configuration</h2><p class='card-subtitle'>Upload a previously exported <code>.json</code> backup to restore recognized settings. Missing fields keep their current values, and obsolete fields are ignored so older backups can still be used after firmware upgrades.</p></div>");

  if (showForm)
  {
    html += F("<form class='upload-form' method='POST' action='");
    html += kConfigImportPath;
    html += F("' enctype='multipart/form-data'>");
    html += F("<section class='upload-field'><h3>Select Backup File</h3><p>Choose the exported configuration <code>.json</code> file. A successful restore will save the imported settings and reboot the clock so network and portal changes take effect cleanly.</p><input id='config-upload' type='file' name='config' accept='.json,application/json' required></section>");
    html += F("<div class='upload-actions'><button type='submit'>Import Configuration</button><a class='button-link ghost' href='/'>Cancel</a></div></form>");
  }

  html += F("<ul class='upload-list'>");
  html += F("<li>Backups are portable across firmware versions because restore looks up settings by stable IDs instead of raw EEPROM positions.</li>");
  html += F("<li>Unknown fields from older or newer firmware are ignored instead of breaking the restore.</li>");
  html += F("<li>After a successful import, the clock reboots so Wi-Fi credentials, portal credentials, and derived state all restart together.</li>");
  html += F("</ul></section></main></div></body></html>");
}

/** Renders the live web console backed by the in-memory console log buffer. */
void appendConsolePage(String &html, const String &accessToken)
{
  String snapshot;
  uint32_t cursor = 0;
  bool truncated = false;
  readConsoleLogSnapshot(snapshot, cursor, truncated);

  appendDocumentHead(html, "LED Smart Clock Console", false);
  html += F("<div class='shell'><header class='hero'>");
  html += F("<p class='eyebrow'>LED Smart Clock Console</p>");
  html += F("<h1>Live Debug Console</h1>");
  html += F("<p class='lede'>This page shows the in-memory runtime console buffer, keeps polling for new output, and can send the same one-character debug commands that normally go over USB serial.</p><div class='action-row'>");
  html += F("<a class='button-link primary' href='");
  html += kDiagnosticsPath;
  html += F("'>Diagnostics</a>");
  html += F("<a class='button-link secondary' href='/'>Dashboard</a>");
  html += F("<a class='button-link secondary' href='");
  html += kConsoleDownloadPath;
  html += F("'>Download Log</a>");
  html += F("<a class='button-link secondary' href='#console-commands'>Commands</a>");
  html += F("</div><div class='portal-chip-row'>");
  appendStatusChip(html, "Address", activeAddressLabel());
  appendStatusChip(html, "Cursor", htmlEscape(String(cursor)));
  appendStatusChip(html, "Serial Debug", serialdebug.isChecked() ? F("Enabled") : F("Disabled"));
  html += F("</div><div class='metric-grid'>");
  appendMetricCard(html, "Buffered Output", htmlEscape(String(snapshot.length()) + F(" bytes")), "tone-neutral");
  appendMetricCard(html, "Live Polling", F("Every 1.5 seconds"), "tone-good");
  appendMetricCard(html, "Command Path", F("/console/command"), "tone-neutral");
  html += F("</div></header>");

  if (truncated)
    appendNotice(html, "notice-warn", F("The RAM console buffer already wrapped. The console below starts at the newest retained output."));

  html += F("<main id='console-page' class='content-grid console-page-grid' data-console-token='");
  html += htmlEscape(accessToken);
  html += F("'>");

  html += F("<section class='card card-span'><div class='card-header'><h2>Live Output</h2><p class='card-subtitle'>Newest output appears here automatically. Use the download button to save the current retained tail as a text file.</p></div><div class='console-actions'>");
  html += F("<a class='button-link primary' href='");
  html += kConsoleDownloadPath;
  if (accessToken.length() > 0)
  {
    html += F("?token=");
    html += htmlEscape(accessToken);
  }
  html += F("'>Download Console Log</a>");
  html += F("<button type='button' class='button-link danger' data-console-clear='true'>Clear Console Buffer</button>");
  html += F("<span class='status-chip'><strong>Incremental Feed</strong><span><code>/console/log</code></span></span>");
  html += F("<span class='status-chip'><strong>Buffer Scope</strong><span>Recent runtime output retained in RAM only</span></span>");
  html += F("</div><pre id='console-output' class='console-view' data-cursor='");
  html += cursor;
  html += F("'>");
  html += htmlEscape(snapshot);
  html += F("</pre><p id='console-status' class='console-status'>Connecting to live console feed...</p></section>");

  html += F("<section id='console-commands' class='card card-span'><div class='card-header'><h2>Send Debug Commands</h2><p class='card-subtitle'>Commands map to the existing serial shortcuts. The first non-space character is used, so enter values such as h, d, g, or s.</p></div><dl class='kv-list'>");
  appendKeyValueRow(html, "Common Commands", F("<code>h</code> help, <code>d</code> dump debug, <code>g</code> GPS status, <code>n</code> raw NMEA, <code>p</code> GPS reset, <code>s</code> coroutine states, <code>l</code> debug logging"));
  appendKeyValueRow(html, "Display Tests", F("<code>a</code> AQI scroller, <code>w</code> current weather, <code>q</code> daily weather, <code>x</code> alert scroller, <code>y</code> temp + icon, <code>e</code> date, <code>t</code> alert flash"));
  appendKeyValueRow(html, "Schedule Impact", F("<code>a</code>, <code>w</code>, <code>q</code>, <code>x</code>, and <code>y</code> run one-shot tests without changing the next scheduled display time."));
  appendKeyValueRow(html, "Receiver Recovery", F("<code>p</code> resets the GPS parser and restarts the UART, <code>u</code> restarts only the GPS UART using the configured baud"));
  appendKeyValueRow(html, "Caution", F("<code>r</code> queues a reboot from the web console after the HTTP response returns"));
  html += F("</dl><div class='console-toolbar'>");
  html += F("<button type='button' data-console-command='h'>Help</button>");
  html += F("<button type='button' data-console-command='d'>Debug Dump</button>");
  html += F("<button type='button' data-console-command='g'>GPS Status</button>");
  html += F("<button type='button' data-console-command='w'>Current Weather</button>");
  html += F("<button type='button' data-console-command='q'>Daily Weather</button>");
  html += F("<button type='button' data-console-command='a'>AQI</button>");
  html += F("<button type='button' data-console-command='x'>Alert Scroll</button>");
  html += F("<button type='button' data-console-command='y'>Temp + Icon</button>");
  html += F("<button type='button' data-console-command='n'>Raw NMEA</button>");
  html += F("<button type='button' data-console-command='p'>Reset GPS</button>");
  html += F("<button type='button' data-console-command='s'>Coroutines</button>");
  html += F("<button type='button' data-console-command='l'>Debug Logs</button>");
  html += F("<button type='button' data-console-command='r'>Reboot</button>");
  html += F("</div><form id='console-command-form' class='console-command-form'><div class='console-command-row'><input id='console-command' name='cmd' type='text' maxlength='8' placeholder='Enter a command such as w, q, a, x, y, g, n, or r'><button type='submit'>Send Command</button></div></form>");
  html += F("<p class='console-note'>Web commands use the same handlers as the USB serial console, and their output is written back into this same RAM log buffer.</p></section>");
  html += F("</main><script>");
  html += FPSTR(kConsolePageScript);
  html += F("</script></div></body></html>");
}

/** Serves the shared stylesheet used by the dashboard, config portal, and console pages. */
void handleThemeCss()
{
  String css;
  css.reserve(22000);
  css += FPSTR(kWebThemeCss);
  css += FPSTR(kPortalConsoleCss);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/css; charset=UTF-8", css);
}

/** Renders the live diagnostics shell contents that can be refreshed in place. */
void appendDiagnosticsContent(String &html)
{
  const acetime_t now = systemClock.getNow();
  const uint32_t nowMillis = millis();
  const size_t healthyCount = countDiagnostics(true, true);
  const size_t pendingCount = countPendingDiagnostics();
  const size_t disabledCount = countDisabledDiagnostics();
  const size_t attentionCount = countAttentionDiagnostics();
  const String gpsRawSnapshot = getGpsRawNmeaSnapshot();
  const String gpsLastByteAge = gps.moduleDetected ? elapsedTime((nowMillis - gps.lastByteMillis + 999UL) / 1000UL) : String(F("Never"));
  const String gpsLastResetAge = gps.lastResetMillis == 0 ? String(F("Never"))
                                                          : elapsedTime((nowMillis - gps.lastResetMillis + 999UL) / 1000UL);

  html += F("<header class='hero'>");
  html += F("<p class='eyebrow'>LED Smart Clock Diagnostics</p>");
  html += F("<h1>Service Health</h1>");
  html += F("<p class='lede'>This page tracks the live state of Wi-Fi, timekeeping, GPS, weather, alerts, air quality, and location services so field debugging does not require a serial console for every issue.</p><div class='action-row'>");
  html += F("<a class='button-link primary' href='/'>Dashboard</a>");
  html += F("<a class='button-link secondary' href='/config'>Configuration</a>");
  html += F("<a class='button-link secondary' href='");
  html += kConsolePath;
  html += F("'>Console</a>");
  html += F("<a class='button-link secondary' href='");
  html += kFirmwareUpdatePath;
  html += F("'>Firmware Update</a>");
  html += F("<a class='button-link ghost' href='");
  html += kLogoutPath;
  html += F("'>Log Out</a>");
  html += F("</div><div class='portal-chip-row'>");
  appendStatusChip(html, "State", htmlEscape(currentConnectionStateLabel()));
  appendStatusChip(html, "Address", activeAddressLabel());
  appendStatusChip(html, "Location", currentLocationLabel());
  html += F("</div><div class='metric-grid'>");
  appendMetricCard(html, "Healthy", htmlEscape(String(healthyCount)), "tone-good");
  appendMetricCard(html, "Attention", htmlEscape(String(attentionCount)), attentionCount > 0 ? "tone-bad" : "tone-neutral");
  appendMetricCard(html, "Waiting", htmlEscape(String(pendingCount)), pendingCount > 0 ? "tone-warn" : "tone-neutral");
  appendMetricCard(html, "Disabled", htmlEscape(String(disabledCount)), disabledCount > 0 ? "tone-neutral" : "tone-good");
  html += F("</div></header>");

  if (attentionCount > 0)
    appendNotice(html, "notice-bad", F("One or more services have recorded a recent failure. Check the red diagnostics cards below for the latest error context."));
  else if (pendingCount > 0)
    appendNotice(html, "notice-warn", F("Some services are still waiting on Wi-Fi, coordinates, API keys, or first successful data. Waiting states are normal during boot and setup."));

  html += F("<p id='diagnostics-action-status' class='console-status'>Display and GPS actions run immediately without changing the normal schedule.</p>");
  html += F("<main class='content-grid'>");

  appendTonedCardStart(html, "Connectivity", "Current network state, captive portal mode, and HTTP transport readiness.", diagnosticTone(DiagnosticService::Wifi));
  appendKeyValueRow(html, "Status", diagnosticSummary(DiagnosticService::Wifi));
  appendKeyValueRow(html, "Detail", diagnosticDetail(DiagnosticService::Wifi));
  appendKeyValueRow(html, "Connection State", htmlEscape(currentConnectionStateLabel()));
  appendKeyValueRow(html, "Address", activeAddressLabel());
  appendKeyValueRow(html, "Captive Portal", isSetupPortalState() ? F("Active") : F("Off"));
  appendKeyValueRow(html, "HTTP Client Ready", isHttpReady() ? F("Yes") : F("No"));
  appendKeyValueRow(html, "Last Success", formatAgo(now, serviceDiagnostic(DiagnosticService::Wifi).lastSuccess));
  appendKeyValueRow(html, "Last Failure", formatAgo(now, serviceDiagnostic(DiagnosticService::Wifi).lastFailure));
  appendCardEnd(html);

  appendTonedCardStart(html, "Timekeeping", "Clock source, SNTP state, timezone rules, and the most recent synchronization path.", diagnosticTone(DiagnosticService::Ntp));
  appendKeyValueRow(html, "Clock Status", htmlEscape(String(clock_status[systemClock.getSyncStatusCode()])));
  appendKeyValueRow(html, "Clock Source", timeSourceLabel());
  appendKeyValueRow(html, "Clock Detail", diagnosticDetail(DiagnosticService::Timekeeping));
  appendKeyValueRow(html, "Last Time Sync", formatAgo(now, systemClock.getLastSyncTime()));
  appendKeyValueRow(html, "NTP Status", diagnosticSummary(DiagnosticService::Ntp));
  appendKeyValueRow(html, "NTP Detail", diagnosticDetail(DiagnosticService::Ntp));
  appendKeyValueRow(html, "NTP Server", ntpServerLabel());
  appendKeyValueRow(html, "NTP Source", ntpServerSourceLabel());
  appendKeyValueRow(html, "NTP Last Success", formatAgo(now, serviceDiagnostic(DiagnosticService::Ntp).lastSuccess));
  appendKeyValueRow(html, "NTP Last Failure", formatAgo(now, serviceDiagnostic(DiagnosticService::Ntp).lastFailure));
  appendKeyValueRow(html, "NTP Last Code", diagnosticCodeLabel(DiagnosticService::Ntp));
  appendKeyValueRow(html, "Timezone", htmlEscape(getSystemTimezoneName()));
  appendKeyValueRow(html, "Timezone Offset", htmlEscape(getSystemTimezoneOffsetString()));
  appendKeyValueRow(html, "DST Active", isSystemTimezoneDstActive() ? F("Yes") : F("No"));
  appendCardEnd(html);

  appendTonedCardStart(html, "GPS", "Receiver UART activity, fix acquisition, and the live navigation data currently available.", diagnosticTone(DiagnosticService::Gps));
  appendKeyValueRow(html, "Status", diagnosticSummary(DiagnosticService::Gps));
  appendKeyValueRow(html, "Detail", diagnosticDetail(DiagnosticService::Gps));
  appendKeyValueRow(html, "Configured Baud", htmlEscape(String(gpsConfiguredBaud())));
  appendKeyValueRow(html, "Active UART Baud", htmlEscape(String(gpsActiveBaud())));
  appendKeyValueRow(html, "Module Detected", gps.moduleDetected ? F("Yes") : F("No"));
  appendKeyValueRow(html, "Last UART Byte", htmlEscape(gpsLastByteAge));
  appendKeyValueRow(html, "Fix", gps.fix ? F("Yes") : F("No"));
  appendKeyValueRow(html, "Waiting For Fix", gps.waitingForFix ? F("Yes") : F("No"));
  appendKeyValueRow(html, "Satellites", htmlEscape(String(gps.sats)));
  appendKeyValueRow(html, "Coordinates", htmlEscape(String(gps.lat, 5) + F(", ") + String(gps.lon, 5)));
  appendKeyValueRow(html, "HDOP", htmlEscape(String(gps.hdop)));
  appendKeyValueRow(html, "Elevation", htmlEscape(String(gps.elevation) + F(" ft")));
  appendKeyValueRow(html, "Parsed Chars", htmlEscape(formatLargeNumber(GPS.charsProcessed())));
  appendKeyValueRow(html, "Sentences With Fix", htmlEscape(formatLargeNumber(GPS.sentencesWithFix())));
  appendKeyValueRow(html, "Parser Resets", htmlEscape(String(gps.parserResetCount)));
  appendKeyValueRow(html, "UART Restarts", htmlEscape(String(gps.uartRestartCount)));
  appendKeyValueRow(html, "Last Reset", htmlEscape(gpsLastResetAge));
  appendKeyValueRow(html, "Reset Reason", safeText(String(gps.lastResetReason), "None"));
  appendKeyValueRow(html, "Last Success", formatAgo(now, serviceDiagnostic(DiagnosticService::Gps).lastSuccess));
  appendKeyValueRow(html, "Last Failure", formatAgo(now, serviceDiagnostic(DiagnosticService::Gps).lastFailure));
  html += F("<div class='console-toolbar gps-actions'>");
  html += F("<button type='button' data-gps-action='restart_uart'>Restart UART</button>");
  html += F("<button type='button' data-gps-action='reset_parser'>Reset Parser &amp; UART</button>");
  html += F("<button type='button' data-gps-action='clear_raw'>Clear Raw NMEA</button>");
  html += F("</div>");
  appendCardEnd(html);

  appendTonedCardStart(html, "GPS Raw NMEA", "Recent raw NMEA traffic captured from the GPS UART. Use it to confirm the receiver is alive, the baud is correct, and complete sentences are arriving.", diagnosticTone(DiagnosticService::Gps));
  appendKeyValueRow(html, "Retained Bytes", htmlEscape(String(gpsRawNmeaLength())));
  appendKeyValueRow(html, "Captured Bytes", htmlEscape(formatLargeNumber(static_cast<int>(gps.rawBytesCaptured))));
  appendKeyValueRow(html, "Captured Sentences", htmlEscape(formatLargeNumber(static_cast<int>(gps.rawSentenceCount))));
  html += F("<pre id='gps-raw-output' class='console-view gps-raw-view'>");
  html += htmlEscape(gpsRawSnapshot.length() > 0 ? gpsRawSnapshot : String(F("No raw NMEA captured yet.")));
  html += F("</pre><p id='gps-raw-status' class='console-status'>Connecting to live GPS raw view...</p>");
  appendCardEnd(html);

  appendTonedCardStart(html, "Weather", "Current conditions refresh health and the data used by the current-weather and daily-weather displays.", diagnosticTone(DiagnosticService::Weather));
  appendKeyValueRow(html, "Status", diagnosticSummary(DiagnosticService::Weather));
  appendKeyValueRow(html, "Detail", diagnosticDetail(DiagnosticService::Weather));
  appendKeyValueRow(html, "Current Conditions", currentWeatherDescription());
  appendKeyValueRow(html, "Current Temperature", formatTemperature(weather.current.temp));
  appendKeyValueRow(html, "Enabled", show_current_temp.isChecked() || show_current_weather.isChecked() || show_daily_weather.isChecked() ? F("Yes") : F("No"));
  appendKeyValueRow(html, "Retries", htmlEscape(String(serviceDiagnostic(DiagnosticService::Weather).retries)));
  appendKeyValueRow(html, "Last Success", formatAgo(now, serviceDiagnostic(DiagnosticService::Weather).lastSuccess));
  appendKeyValueRow(html, "Last Failure", formatAgo(now, serviceDiagnostic(DiagnosticService::Weather).lastFailure));
  appendKeyValueRow(html, "Last Code", diagnosticCodeLabel(DiagnosticService::Weather));
  appendKeyValueRow(html, "Next Weather Scroll", formatUntil(now, lastshown.currentweather, static_cast<uint32_t>(current_weather_interval.value()) * 3600U));
  html += F("<div class='console-toolbar'>");
  html += F("<button type='button' data-display-test='current_weather'>Test Current Weather</button>");
  html += F("<button type='button' data-display-test='daily_weather'>Test Daily Forecast</button>");
  html += F("<button type='button' data-display-test='temp_icon'>Test Temp + Icon</button>");
  html += F("</div>");
  appendCardEnd(html);

  appendTonedCardStart(html, "Air Quality", "AQI forecast refresh health and the pollutant summary used by the AQI display block.", diagnosticTone(DiagnosticService::AirQuality));
  appendKeyValueRow(html, "Status", diagnosticSummary(DiagnosticService::AirQuality));
  appendKeyValueRow(html, "Detail", diagnosticDetail(DiagnosticService::AirQuality));
  appendKeyValueRow(html, "Current AQI", htmlEscape(String(air_quality[aqi.current.aqi])));
  appendKeyValueRow(html, "Description", currentAqiDescription());
  appendKeyValueRow(html, "Enabled", show_aqi.isChecked() ? F("Yes") : F("No"));
  appendKeyValueRow(html, "Retries", htmlEscape(String(serviceDiagnostic(DiagnosticService::AirQuality).retries)));
  appendKeyValueRow(html, "Last Success", formatAgo(now, serviceDiagnostic(DiagnosticService::AirQuality).lastSuccess));
  appendKeyValueRow(html, "Last Failure", formatAgo(now, serviceDiagnostic(DiagnosticService::AirQuality).lastFailure));
  appendKeyValueRow(html, "Last Code", diagnosticCodeLabel(DiagnosticService::AirQuality));
  appendKeyValueRow(html, "Next AQI Scroll", formatUntil(now, lastshown.aqi, static_cast<uint32_t>(aqi_interval.value()) * 60U));
  html += F("<div class='console-toolbar'>");
  html += F("<button type='button' data-display-test='aqi'>Test AQI Scroll</button>");
  html += F("</div>");
  appendCardEnd(html);

  appendTonedCardStart(html, "Alerts", "weather.gov polling health and the currently selected watch or warning text.", diagnosticTone(DiagnosticService::Alerts));
  appendKeyValueRow(html, "Status", diagnosticSummary(DiagnosticService::Alerts));
  appendKeyValueRow(html, "Detail", diagnosticDetail(DiagnosticService::Alerts));
  appendKeyValueRow(html, "Alert Active", alerts.active ? F("Yes") : F("No"));
  appendKeyValueRow(html, "Tracked Alerts", htmlEscape(String(alerts.count)));
  appendKeyValueRow(html, "Warning Count", htmlEscape(String(alerts.warningCount)));
  appendKeyValueRow(html, "Watch Count", htmlEscape(String(alerts.watchCount)));
  appendKeyValueRow(html, "Primary Event", currentAlertEvent());
  appendKeyValueRow(html, "Active Set", activeAlertsLabel());
  appendKeyValueRow(html, "Description", currentAlertDescription());
  appendKeyValueRow(html, "Matrix Text", currentAlertDisplayText());
  appendKeyValueRow(html, "Retries", htmlEscape(String(serviceDiagnostic(DiagnosticService::Alerts).retries)));
  appendKeyValueRow(html, "Last Success", formatAgo(now, serviceDiagnostic(DiagnosticService::Alerts).lastSuccess));
  appendKeyValueRow(html, "Last Failure", formatAgo(now, serviceDiagnostic(DiagnosticService::Alerts).lastFailure));
  appendKeyValueRow(html, "Last Code", diagnosticCodeLabel(DiagnosticService::Alerts));
  html += F("<div class='console-toolbar'>");
  html += F("<button type='button' data-display-test='alerts'>Test Alert Scroll</button>");
  html += F("</div>");
  appendCardEnd(html);

  appendTonedCardStart(html, "IP Geolocation", "Public-IP-based timezone and coarse coordinate fallback state.", diagnosticTone(DiagnosticService::IpGeo));
  appendKeyValueRow(html, "Status", diagnosticSummary(DiagnosticService::IpGeo));
  appendKeyValueRow(html, "Detail", diagnosticDetail(DiagnosticService::IpGeo));
  appendKeyValueRow(html, "Timezone", safeText(String(ipgeo.timezone), "Waiting"));
  appendKeyValueRow(html, "Coordinates", htmlEscape(String(ipgeo.lat, 5) + F(", ") + String(ipgeo.lon, 5)));
  appendKeyValueRow(html, "Retries", htmlEscape(String(serviceDiagnostic(DiagnosticService::IpGeo).retries)));
  appendKeyValueRow(html, "Last Success", formatAgo(now, serviceDiagnostic(DiagnosticService::IpGeo).lastSuccess));
  appendKeyValueRow(html, "Last Failure", formatAgo(now, serviceDiagnostic(DiagnosticService::IpGeo).lastFailure));
  appendKeyValueRow(html, "Last Code", diagnosticCodeLabel(DiagnosticService::IpGeo));
  appendCardEnd(html);

  appendTonedCardStart(html, "Reverse Geocode", "City and region resolution based on the active service coordinates.", diagnosticTone(DiagnosticService::Geocode));
  appendKeyValueRow(html, "Status", diagnosticSummary(DiagnosticService::Geocode));
  appendKeyValueRow(html, "Detail", diagnosticDetail(DiagnosticService::Geocode));
  appendKeyValueRow(html, "Location", currentLocationLabel());
  appendKeyValueRow(html, "Coordinates", currentCoordinatesLabel());
  appendKeyValueRow(html, "Location Source", safeText(String(current.locsource), "Unknown"));
  appendKeyValueRow(html, "Retries", htmlEscape(String(serviceDiagnostic(DiagnosticService::Geocode).retries)));
  appendKeyValueRow(html, "Last Success", formatAgo(now, serviceDiagnostic(DiagnosticService::Geocode).lastSuccess));
  appendKeyValueRow(html, "Last Failure", formatAgo(now, serviceDiagnostic(DiagnosticService::Geocode).lastFailure));
  appendKeyValueRow(html, "Last Code", diagnosticCodeLabel(DiagnosticService::Geocode));
  appendCardEnd(html);

  html += F("</main>");
}

/** Renders the detailed diagnostics page with live in-place refresh enabled. */
void appendDiagnosticsPage(String &html)
{
  appendDocumentHead(html, "LED Smart Clock Diagnostics", true);
  appendLiveShellStart(html, "/diagnostics?partial=1");
  appendDiagnosticsContent(html);
  appendLiveShellEnd(html);
  html += F("<script>");
  html += FPSTR(kLiveRefreshScript);
  html += FPSTR(kDiagnosticsPageScript);
  html += F("</script></body></html>");
}

/** Renders the live status shell contents that can be refreshed in place. */
void appendStatusContent(String &html)
{
  acetime_t now = systemClock.getNow();
  const size_t healthyCount = countDiagnostics(true, true);
  const size_t attentionCount = countAttentionDiagnostics();
  const size_t pendingCount = countPendingDiagnostics();
  const size_t disabledCount = countDisabledDiagnostics();

  html += F("<header class='hero'>");
  html += F("<p class='eyebrow'>LED Smart Clock Status</p>");
  html += F("<h1>Runtime Dashboard</h1>");
  html += F("<p class='lede'>");
  html += htmlEscape(getSystemZonedDateTimeString());
  html += F("</p><div class='action-row'>");
  html += F("<a class='button-link primary' href='/config'>Configuration</a>");
  html += F("<a class='button-link secondary' href='");
  html += kDiagnosticsPath;
  html += F("'>Diagnostics</a>");
  html += F("<a class='button-link secondary' href='");
  html += kConsolePath;
  html += F("'>Console</a>");
  html += F("<a class='button-link secondary' href='/firmware'>Firmware Update</a>");
  html += F("<a class='button-link ghost' href='");
  html += kLogoutPath;
  html += F("'>Log Out</a>");
  html += F("<a class='button-link danger' href='/reboot'>Reboot</a>");
  html += F("</div><div class='portal-chip-row'>");
  appendStatusChip(html, "State", htmlEscape(currentConnectionStateLabel()));
  appendStatusChip(html, "Address", activeAddressLabel());
  appendStatusChip(html, "Location", currentLocationLabel());
  html += F("</div><div class='metric-grid'>");
  appendMetricCard(html, "Uptime", htmlEscape(elapsedTime(static_cast<uint32_t>(now - runtimeState.bootTime))), "tone-neutral");
  appendMetricCard(html, "Time Source", timeSourceLabel(),
                   (strcmp(runtimeState.timeSource, "gps") == 0 || strcmp(runtimeState.timeSource, "ntp") == 0)
                       ? "tone-good"
                       : "tone-neutral");
  appendMetricCard(html, "Brightness", brightnessPercentLabel(), "tone-neutral");
  appendMetricCard(html, "Air Quality", htmlEscape(String(air_quality[aqi.current.aqi])), aqiTone());
  appendMetricCard(html, "Weather Alerts", alerts.active ? htmlEscape(String(alerts.count) + F(" active")) : String(F("Clear")), alertTone());
  html += F("</div></header>");

  if (!isApiValid(ipgeoapi.value()))
    appendNotice(html, "notice-warn", F("ipgeolocation.io is not configured. Automatic timezone and fallback location updates will stay limited until you add a valid API key."));
  if (!isApiValid(weatherapi.value()))
    appendNotice(html, "notice-warn", F("OpenWeather is not configured. Current weather, daily forecast, and AQI data will remain unavailable until a One Call 3.0 API key is saved."));
  if (!isCoordsValid())
    appendNotice(html, "notice-bad", F("The clock does not currently have valid coordinates, so remote weather and air-quality lookups cannot complete yet."));
  if (attentionCount > 0)
    appendNotice(html, "notice-bad", F("One or more live services need attention right now. The service-health summary below highlights the failing subsystem and the latest status text."));
  else if (pendingCount > 0)
    appendNotice(html, "notice-warn", F("Some services are still waiting on first data, coordinates, Wi-Fi, or API readiness. That can be normal during boot, but the health summary below shows which service is still pending."));

  html += F("<main class='content-grid'>");

  html += F("<section class='card card-span'><div class='card-header'><h2>Service Health</h2><p class='card-subtitle'>Quick runtime triage for the major online and sensor subsystems. Open the full diagnostics page for retries, last codes, and deeper timing detail.</p></div><div class='metric-grid'>");
  appendMetricCard(html, "Healthy", htmlEscape(String(healthyCount)), "tone-good");
  appendMetricCard(html, "Attention", htmlEscape(String(attentionCount)), attentionCount > 0 ? "tone-bad" : "tone-neutral");
  appendMetricCard(html, "Waiting", htmlEscape(String(pendingCount)), pendingCount > 0 ? "tone-warn" : "tone-neutral");
  appendMetricCard(html, "Disabled", htmlEscape(String(disabledCount)), disabledCount > 0 ? "tone-neutral" : "tone-good");
  html += F("</div><div class='service-health-grid'>");
  for (size_t index = 0; index < kDiagnosticServiceCount; ++index)
    appendServiceHealthCard(html, static_cast<DiagnosticService>(index), now);
  html += F("</div><div class='action-row'><a class='button-link primary' href='");
  html += kDiagnosticsPath;
  html += F("'>Open Full Diagnostics</a><a class='button-link ghost' href='");
  html += kConsolePath;
  html += F("'>Open Console</a></div></section>");

  appendCardStart(html, "System", "Core firmware identity, connectivity, and synchronization.");
  appendKeyValueRow(html, "Firmware Version", htmlEscape(String(VERSION_SEMVER)));
  appendKeyValueRow(html, "Connection State", htmlEscape(currentConnectionStateLabel()));
  appendKeyValueRow(html, "Current Address", activeAddressLabel());
  appendKeyValueRow(html, "Uptime", htmlEscape(elapsedTime(static_cast<uint32_t>(now - runtimeState.bootTime))));
  appendKeyValueRow(html, "Last Time Sync", formatAgo(now, systemClock.getLastSyncTime()));
  appendKeyValueRow(html, "Time Source", timeSourceLabel());
  appendKeyValueRow(html, "NTP Server", dashboardClampValue(ntpServerLabel()));
  appendKeyValueRow(html, "NTP Source", dashboardClampValue(ntpServerSourceLabel()));
  appendKeyValueRow(html, "Timezone", dashboardClampValue(htmlEscape(getSystemTimezoneName())));
  appendKeyValueRow(html, "Timezone Source", timezoneSourceLabel());
  appendKeyValueRow(html, "Timezone Offset", htmlEscape(getSystemTimezoneOffsetString()));
  appendKeyValueRow(html, "DST Active", isSystemTimezoneDstActive() ? F("Yes") : F("No"));
  appendCardEnd(html);

  appendCardStart(html, "Location", "Resolved city and coordinate data used by the external services.");
  appendKeyValueRow(html, "Location", dashboardClampValue(currentLocationLabel()));
  appendKeyValueRow(html, "Coordinates", currentCoordinatesLabel());
  appendKeyValueRow(html, "Location Source", dashboardClampValue(safeText(String(current.locsource), "Unknown")));
  appendKeyValueRow(html, "IP Geo Success", formatAgo(now, checkipgeo.lastsuccess));
  appendKeyValueRow(html, "Geocode Success", formatAgo(now, checkgeocode.lastsuccess));
  appendKeyValueRow(html, "Saved City", dashboardClampValue(isLocationValid("saved") ? safeText(String(savedcity.value()), "Not saved") : String(F("Not saved"))));
  appendCardEnd(html);

  appendCardStart(html, "GPS", "Live fix quality and parser statistics from the attached receiver.");
  appendKeyValueRow(html, "GPS Fix", gps.fix ? F("Yes") : F("No"));
  appendKeyValueRow(html, "Satellites", htmlEscape(String(gps.sats)));
  appendKeyValueRow(html, "Fix Age", formatAgo(now, gps.timestamp));
  appendKeyValueRow(html, "Location Age", formatAgo(now, gps.lockage));
  appendKeyValueRow(html, "Precision", htmlEscape(String(gps.hdop) + F(" m")));
  appendKeyValueRow(html, "Elevation", htmlEscape(String(gps.elevation) + F(" m")));
  appendKeyValueRow(html, "Parsed Sentences", htmlEscape(formatLargeNumber(GPS.charsProcessed())));
  appendCardEnd(html);

  appendCardStart(html, "Weather", "Current conditions, daily outlook, and display rotation timing.");
  appendKeyValueRow(html, "Current Temperature", formatTemperature(weather.current.temp));
  appendKeyValueRow(html, "Feels Like", formatTemperature(weather.current.feelsLike));
  appendKeyValueRow(html, "Conditions", dashboardClampValue(currentWeatherDescription()));
  appendKeyValueRow(html, "Temp Display Duration", htmlEscape(String(current_temp_duration.value()) + F(" seconds")));
  appendKeyValueRow(html, "Humidity", htmlEscape(String(weather.current.humidity) + F("%")));
  appendKeyValueRow(html, "Wind", htmlEscape(String(weather.current.windSpeed) + F(" / ") + String(weather.current.windGust) + F(" ")) + speedUnitLabel());
  appendKeyValueRow(html, "Daily Range", formatTemperature(weather.day.tempMax) + F(" high / ") + formatTemperature(weather.day.tempMin) + F(" low"));
  appendKeyValueRow(html, "Forecast Success", formatAgo(now, checkweather.lastsuccess));
  appendKeyValueRow(html, "Next Weather Scroll", formatUntil(now, lastshown.currentweather, static_cast<uint32_t>(current_weather_interval.value()) * 3600U));
  appendKeyValueRow(html, "Next Daily Scroll", formatUntil(now, lastshown.dayweather, static_cast<uint32_t>(daily_weather_interval.value()) * 3600U));
  appendCardEnd(html);

  appendCardStart(html, "Air Quality", "Current AQI bucket, pollutant summary, and rotation timing.");
  appendKeyValueRow(html, "Current AQI", htmlEscape(String(air_quality[aqi.current.aqi])));
  appendKeyValueRow(html, "AQI Description", dashboardClampValue(currentAqiDescription()));
  appendKeyValueRow(html, "PM2.5 / PM10", htmlEscape(String(aqi.current.pm25, 1) + F(" / ") + String(aqi.current.pm10, 1)));
  appendKeyValueRow(html, "Ozone / Nitrogen Dioxide", htmlEscape(String(aqi.current.o3, 1) + F(" / ") + String(aqi.current.no2, 1)));
  appendKeyValueRow(html, "AQI Success", formatAgo(now, checkaqi.lastsuccess));
  appendKeyValueRow(html, "Next AQI Scroll", formatUntil(now, lastshown.aqi, static_cast<uint32_t>(aqi_interval.value()) * 60U));
  appendCardEnd(html);

  appendCardStart(html, "Alerts", "Weather.gov alert state and recent delivery timing.");
  appendKeyValueRow(html, "Alert Active", alerts.active ? F("Yes") : F("No"));
  appendKeyValueRow(html, "Tracked Alerts", htmlEscape(String(alerts.count)));
  appendKeyValueRow(html, "Warning Count", htmlEscape(String(alerts.warningCount)));
  appendKeyValueRow(html, "Watch Count", htmlEscape(String(alerts.watchCount)));
  appendKeyValueRow(html, "Primary Event", dashboardClampValue(currentAlertEvent()));
  appendKeyValueRow(html, "Active Set", dashboardClampValue(activeAlertsLabel()));
  appendKeyValueRow(html, "Alert Description", dashboardClampValue(currentAlertDescription()));
  appendKeyValueRow(html, "Matrix Text", dashboardClampValue(currentAlertDisplayText()));
  appendKeyValueRow(html, "Alert Success", formatAgo(now, checkalerts.lastsuccess));
  appendKeyValueRow(html, "Last Alert Shown", formatAgo(now, lastshown.alerts));
  appendCardEnd(html);

  html += F("</main>");
}

/** Renders the main status dashboard shown during normal operation. */
void appendStatusPage(String &html)
{
  appendDocumentHead(html, "LED Smart Clock Status", true);
  appendLiveShellStart(html, "/?partial=1");
  appendStatusContent(html);
  appendLiveShellEnd(html);
  html += F("<script>");
  html += FPSTR(kLiveRefreshScript);
  html += F("</script></body></html>");
}

/** Customizes the generated IotWebConf portal so it matches the status page. */
class ClockHtmlFormatProvider : public iotwebconf::HtmlFormatProvider
{
public:
  String getHead() override
  {
    String html;
    html.reserve(192);
    html += F("<!DOCTYPE html><html lang='en' data-web-theme='");
    html += (web_dark_mode.isChecked() ? F("dark") : F("light"));
    html += F("'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'><title>LED Smart Clock Configuration</title>");
    return html;
  }

  String getStyle() override
  {
    return String(F("<link rel='stylesheet' href='")) + kThemeCssPath + F("?v=") + VERSION_SEMVER + F("'>");
  }

  String getStyleInner() override
  {
    return String();
  }

  String getScriptInner() override
  {
    return HtmlFormatProvider::getScriptInner() + String(FPSTR(kConfigPortalScript));
  }

  String getHeadExtension() override
  {
    return web_dark_mode.isChecked()
               ? String(F("<meta name='theme-color' content='#101d28'>"))
               : String(F("<meta name='theme-color' content='#173748'>"));
  }

  String getBodyInner() override
  {
  String html;
  html.reserve(1800);
  html += F("<div class='portal-shell'><header class='portal-hero'>");
    html += F("<p class='portal-eyebrow'>Firmware v");
    html += VERSION_SEMVER;
    html += F("</p><h1>Configuration</h1><p class='portal-lede'>Tune Wi-Fi, display behavior, weather services, alerts, and location rules from a single cohesive control surface.</p><div class='portal-chip-row'>");
  appendStatusChip(html, "State", htmlEscape(currentConnectionStateLabel()));
  appendStatusChip(html, "Address", activeAddressLabel());
    html += F("</div><div class='action-row'>");
    html += F("<a class='button-link primary' href='/'>Dashboard</a>");
    html += F("<a class='button-link secondary' href='");
    html += kDiagnosticsPath;
    html += F("'>Diagnostics</a>");
    html += F("<a class='button-link secondary' href='");
    html += kConsolePath;
    html += F("'>Console</a>");
    html += F("<a class='button-link ghost' href='");
    html += kLogoutPath;
    html += F("'>Log Out</a></div></header><div id='portal-runtime' hidden data-ntp-server='");
  html += ntpServerLabel();
  html += F("' data-ntp-source='");
  html += ntpServerSourceLabel();
  html += F("'></div><div class='portal-mode-bar'><p class='portal-mode-help'>Basic mode highlights the settings most people change often. Advanced mode reveals every saved option and maintenance control.</p><div class='portal-mode-toggle'><button type='button' data-config-view='basic'>Basic Mode</button><button type='button' data-config-view='advanced'>Advanced Mode</button></div></div><nav class='portal-nav' id='portal-nav-links'></nav>");
    html += F("<section class='portal-card portal-intro'><div class='card-header'><h2>Backup & Restore</h2><p class='card-subtitle'>Download a JSON backup before firmware work or major changes, then use restore to seed this clock from a previous setup. Import matches fields by stable IDs, so older backups can still restore the settings this firmware recognizes.</p></div><div class='action-row'>");
    html += F("<a class='button-link primary' href='");
    html += kConfigExportPath;
    html += F("'>Download Config Backup</a><a class='button-link secondary' href='");
    html += kConfigImportPath;
    html += F("'>Restore From Backup</a></div><p>The backup file includes Wi-Fi credentials, portal passwords, and API keys, so store it privately.</p></section>");
    html += F("<section class='portal-card portal-intro'><div class='card-header'><h2>Before You Save</h2><p class='card-subtitle'>Changes are written to flash when you apply them. Wi-Fi or API edits can briefly interrupt online services while the clock reconnects and refreshes its derived state.</p></div>");
    html += F("<p>Use the status page at <a href='/'>/</a> to confirm runtime basics, and the <a href='");
    html += kDiagnosticsPath;
    html += F("'>diagnostics page</a> when you need detailed service health, retries, and recent error context after saving.</p></section>");
    html += F("<section class='portal-card portal-surface'>");
    return html;
  }

  String getFormStart() override
  {
    return F("<form action='' method='post' class='portal-form'><input type='hidden' name='iotSave' value='true'>");
  }

  String getFormEnd() override
  {
    return F("<div class='portal-actions'><p class='portal-help'>Save once you finish editing a batch of settings. Validation errors will stay inside the affected section. Cancel returns to the dashboard without writing any of the unsaved form changes.</p><div class='portal-button-row'><a class='button-link ghost' href='/'>Cancel</a><a class='button-link ghost' href='/logout'>Log Out</a><a class='button-link danger' href='/reboot'>Reboot</a><button type='submit'>Save Configuration</button></div></div></form>");
  }

  String getUpdate() override
  {
    return F("<div class='portal-meta'><a class='portal-inline-action' href='{u}'>Open Firmware Update</a><p class='portal-meta-note'>Web updates flash the compiled <code>update.bin</code> image into the OTA application slot. First-time installs use the release <code>firmware.bin</code> image through the web installer or USB. If a release changes the partition table or bootloader, those images still need to be flashed over USB.</p></div>");
  }

  String getConfigVer() override { return String(); }

  String getEnd() override
  {
    return F("</section></div></body></html>");
  }
};

ClockHtmlFormatProvider clockHtmlFormatProvider;

/** Convenience overload used by the root handler when no custom onboarding state is needed. */
void appendSetupPage(String &html)
{
  appendSetupPage(html, nullptr, nullptr, String(), false);
}

/** Validates and writes the submitted onboarding values into the live configuration. */
bool applyOnboardingDraft(const OnboardingDraft &draft, String &error)
{
  error = "";

  const String wifiSsid = draft.wifiSsid;
  const String adminPassword = draft.adminPassword;
  const bool enableWebProtection = draft.enableWebPassword;
  if (wifiSsid.length() == 0)
  {
    error = F("Wi-Fi SSID is required before onboarding can finish.");
    return false;
  }
  if (enableWebProtection && adminPassword.length() < 8)
  {
    error = F("Choose a password that is at least 8 characters long.");
    return false;
  }

  if (!copyParameterValue(*iotWebConf.getWifiSsidParameter(), wifiSsid, "Wi-Fi SSID", error) ||
      !copyParameterValue(*iotWebConf.getWifiPasswordParameter(), draft.wifiPassword, "Wi-Fi password", error) ||
      !copyParameterValue(weatherapi, draft.weatherApiKey, "OpenWeather API key", error) ||
      !copyParameterValue(ipgeoapi, draft.ipGeoApiKey, "IPGeolocation API key", error))
  {
    return false;
  }

  web_password_protection.value() = enableWebProtection;
  if (!copyParameterValue(*iotWebConf.getApPasswordParameter(),
                          enableWebProtection ? adminPassword : String(wifiInitialApPassword),
                          "Clock password", error))
  {
    return false;
  }
  enable_fixed_tz.value() = draft.useFixedTimezone;
  fixed_offset.value() = draft.fixedOffset;
  normalizeLoadedConfigValues();
  iotWebConf.saveConfig();
  return true;
}

/** Handles GET requests for the password-only login page. */
void handleLoginPage()
{
  if (iotWebConf.handleCaptivePortal())
    return;

  if (!isProtectedPortalState())
  {
    redirectTo(isSetupPortalState() ? String(kOnboardingPath) : String("/"));
    return;
  }

  if (hasValidWebSession())
  {
    redirectTo(requestedNextPath());
    return;
  }

  String html;
  html.reserve(7000);
  appendLoginPage(html,
                  server.hasArg("logged_out") ? "notice-good" : nullptr,
                  server.hasArg("logged_out") ? String(F("You have been signed out of the clock web interface.")) : String(),
                  requestedNextPath());
  server.send(200, "text/html; charset=UTF-8", html);
}

/** Handles POST requests from the password-only login form. */
void handleLoginPost()
{
  if (iotWebConf.handleCaptivePortal())
    return;

  if (!isProtectedPortalState())
  {
    redirectTo(isSetupPortalState() ? String(kOnboardingPath) : String("/"));
    return;
  }

  String html;
  html.reserve(7000);
  const String submittedPassword = server.hasArg("password") ? server.arg("password") : String();
  const String nextPath = requestedNextPath();
  if (submittedPassword == String(iotWebConf.getApPasswordParameter()->valueBuffer) &&
      submittedPassword.length() > 0)
  {
    issueWebSession();
    redirectTo(nextPath);
    return;
  }

  appendLoginPage(html, "notice-bad", F("The supplied password did not match this clock."), nextPath);
  server.send(401, "text/html; charset=UTF-8", html);
}

/** Clears the current browser session and returns the user to the login screen. */
void handleLogout()
{
  clearWebSessionCookie();
  String html;
  html.reserve(7000);
  appendLoginPage(html, "notice-good", F("You have been signed out of the clock web interface."), String("/"));
  server.send(200, "text/html; charset=UTF-8", html);
}

/** Handles GET requests for first-boot or recovery-mode onboarding. */
void handleOnboarding()
{
  if (iotWebConf.handleCaptivePortal())
    return;

  if (!isSetupPortalState())
  {
    if (!authorizeAdminRequest())
      return;
    redirectTo(String("/"));
    return;
  }

  String html;
  html.reserve(18000);
  const bool showSelfTest = server.hasArg("saved") && server.arg("saved") == "1";
  appendSetupPage(html, nullptr,
                  showSelfTest ? "notice-good" : nullptr,
                  showSelfTest ? String(F("Onboarding values were saved. Disconnect from the setup access point when you are ready for the clock to join your configured Wi-Fi network.")) : String(),
                  showSelfTest);
  server.send(200, "text/html; charset=UTF-8", html);
}

/** Handles POST submissions from the onboarding wizard. */
void handleOnboardingPost()
{
  if (iotWebConf.handleCaptivePortal())
    return;

  if (!isSetupPortalState())
  {
    if (!authorizeAdminRequest())
      return;
    redirectTo(String("/"));
    return;
  }

  OnboardingDraft draft = submittedOnboardingDraft();
  String error;
  if (!applyOnboardingDraft(draft, error))
  {
    String html;
    html.reserve(18000);
    appendSetupPage(html, &draft, "notice-bad", error, false);
    server.send(400, "text/html; charset=UTF-8", html);
    return;
  }

  String html;
  html.reserve(18000);
  appendSetupPage(html, &draft, "notice-good",
                  F("Onboarding values were saved successfully. Use the self-test below, then disconnect from the setup AP so the clock can finish joining your normal Wi-Fi network."),
                  true);
  server.send(200, "text/html; charset=UTF-8", html);
}

/** Triggers a visible matrix scroll test and redirects back to the onboarding or main UI. */
void handleOnboardingMatrixTest()
{
  if (iotWebConf.handleCaptivePortal())
    return;

  if (isProtectedPortalState() && !authorizeAdminRequest())
    return;

  strlcpy(scrolltext.message, "LED MATRIX SELF TEST", sizeof(scrolltext.message));
  startScroll(hex2rgb(system_color.value()), false);

  String returnPath = server.hasArg("return") ? server.arg("return") : String(kOnboardingPath);
  if (returnPath.length() == 0 || returnPath.charAt(0) != '/' || returnPath.startsWith("//"))
    returnPath = String(kOnboardingPath);
  redirectTo(returnPath);
}

/** Routes the advanced configuration portal through the custom session-aware request wrapper. */
void handleConfigPage()
{
  if (iotWebConf.handleCaptivePortal())
    return;

  SessionWebRequestWrapper requestWrapper;
  iotWebConf.handleConfig(&requestWrapper);
}

/** Captures the latest OTA updater error as readable text for the themed status page. */
void setFirmwareUploadErrorFromUpdater()
{
  firmwareUploadError = String(F("Firmware update failed: ")) + String(Update.errorString());
}

/** Renders the GET view for the themed firmware upload page. */
void handleFirmwareUpdate()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeAdminRequest())
    return;

  resetFirmwareUploadState();
  String html;
  html.reserve(9000);
  appendFirmwareUpdatePage(html, "notice-warn", F("Upload the compiled application image only. A successful OTA update will reboot the clock automatically."), true);
  server.send(200, "text/html; charset=UTF-8", html);
}

/** Finalizes a firmware upload request and returns a themed success or error screen. */
void handleFirmwareUpdatePost()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeAdminRequest())
    return;

  String html;
  html.reserve(9000);

  if (!firmwareUploadStarted || !firmwareUploadAuthenticated || firmwareUploadError.length() > 0)
  {
    String message = firmwareUploadError.length() > 0
                         ? firmwareUploadError
                         : String(F("Firmware upload did not start. Choose an update.bin file and try again."));
    appendFirmwareUpdatePage(html, "notice-bad", htmlEscape(message), true);
    server.send(400, "text/html; charset=UTF-8", html);
    resetFirmwareUploadState();
    return;
  }

  if (Update.hasError() || firmwareUploadError.length() > 0)
  {
    String message = firmwareUploadError.length() > 0 ? firmwareUploadError : String(F("Firmware update failed."));
    appendFirmwareUpdatePage(html, "notice-bad", htmlEscape(message), true);
    server.send(200, "text/html; charset=UTF-8", html);
    resetFirmwareUploadState();
    return;
  }

  runtimeState.rebootRequested = true;
  runtimeState.rebootRequestMillis = millis();
  appendFirmwareUpdatePage(html, "notice-good", F("Firmware upload completed successfully. The clock is rebooting now, and the dashboard should return after the restart finishes."), false);
  server.send(200, "text/html; charset=UTF-8", html);
  resetFirmwareUploadState();
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

    if (!upload.filename.endsWith(".bin"))
    {
      firmwareUploadError = F("Only the compiled update.bin image is supported on this page.");
      return;
    }

    firmwareUploadStarted = true;
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000U) & 0xFFFFF000U;
    if (!Update.begin(maxSketchSpace, U_FLASH))
      setFirmwareUploadErrorFromUpdater();
  }
  else if (firmwareUploadAuthenticated && upload.status == UPLOAD_FILE_WRITE && firmwareUploadError.length() == 0)
  {
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

/** Streams the current configuration as a downloadable JSON attachment. */
void handleConfigExport()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeAdminRequest())
    return;

  String json;
  String error;
  if (!exportConfigurationBackup(json, error))
  {
    String html;
    html.reserve(9000);
    appendConfigBackupPage(html, "notice-bad", htmlEscape(error), true);
    server.send(500, "text/html; charset=UTF-8", html);
    return;
  }

  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Content-Disposition", String(F("attachment; filename=\"")) + configBackupDownloadFileName() + F("\""));
  server.send(200, "application/json; charset=UTF-8", json);
}

/** Renders the GET view for the configuration backup and restore page. */
void handleConfigImport()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeAdminRequest())
    return;

  resetConfigImportState();
  String html;
  html.reserve(9000);
  appendConfigBackupPage(html, "notice-warn", F("Download a fresh backup before firmware work, then use this page to restore a previous setup when you want to seed a new build or recover from a reset."), true);
  server.send(200, "text/html; charset=UTF-8", html);
}

/** Finalizes a posted configuration backup and applies it if the upload was valid. */
void handleConfigImportPost()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeAdminRequest())
    return;

  String html;
  html.reserve(9000);

  if (!configImportStarted || !configImportAuthenticated || configImportError.length() > 0)
  {
    String message = configImportError.length() > 0
                         ? configImportError
                         : String(F("Configuration restore did not start. Choose an exported JSON backup and try again."));
    appendConfigBackupPage(html, "notice-bad", htmlEscape(message), true);
    server.send(400, "text/html; charset=UTF-8", html);
    resetConfigImportState();
    return;
  }

  ConfigImportResult importResult;
  if (!importConfigurationBackup(configImportPayload, importResult))
  {
    appendConfigBackupPage(html, "notice-bad", htmlEscape(importResult.error), true);
    server.send(400, "text/html; charset=UTF-8", html);
    resetConfigImportState();
    return;
  }

  runtimeState.rebootRequested = true;
  runtimeState.rebootRequestMillis = millis();

  String message = importResult.summary + F(" The clock is rebooting now so restored Wi-Fi, portal, and runtime settings all restart together.");
  appendConfigBackupPage(html, importResult.hasWarnings ? "notice-warn" : "notice-good", htmlEscape(message), false);
  server.send(200, "text/html; charset=UTF-8", html);
  resetConfigImportState();
}

/** Collects the uploaded configuration JSON file so it can be restored after auth succeeds. */
void handleConfigImportUpload()
{
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START)
  {
    resetConfigImportState();
    configImportAuthenticated = isAdminSessionAuthorized();

    if (!configImportAuthenticated)
    {
      configImportError = F("Authentication failed before the configuration import could start.");
      return;
    }

    if (upload.name != "config")
    {
      configImportError = F("Unexpected upload field. Choose an exported JSON backup and try again.");
      return;
    }

    if (!upload.filename.endsWith(".json"))
    {
      configImportError = F("Only exported JSON configuration backups are supported on this page.");
      return;
    }

    configImportStarted = true;
    configImportPayload.reserve(kMaxConfigImportBytes);
  }
  else if (configImportAuthenticated && upload.status == UPLOAD_FILE_WRITE && configImportError.length() == 0)
  {
    if (configImportPayload.length() + upload.currentSize > kMaxConfigImportBytes)
    {
      configImportError = F("The uploaded backup file is too large. Expected a compact JSON export from the clock.");
      configImportStarted = false;
      configImportPayload = "";
      return;
    }

    if (!configImportPayload.concat(reinterpret_cast<const char *>(upload.buf), upload.currentSize))
    {
      configImportError = F("The clock ran out of memory while receiving the uploaded backup.");
      configImportStarted = false;
      configImportPayload = "";
    }
  }
  else if (upload.status == UPLOAD_FILE_ABORTED)
  {
    configImportError = F("Configuration import was aborted before completion.");
    configImportStarted = false;
    configImportPayload = "";
  }

  delay(0);
}

/** Renders the live web console page backed by the RAM log buffer. */
void handleGpsRaw()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeAdminRequest())
    return;

  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Pragma", "no-cache");
  server.send(200, "text/plain; charset=UTF-8", getGpsRawNmeaSnapshot());
}

/** Executes one GPS troubleshooting action requested from the diagnostics page. */
void handleGpsAction()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeAdminRequest())
    return;

  String action = server.hasArg("action") ? server.arg("action") : String();
  action.trim();
  action.toLowerCase();

  String message;
  if (action == "restart_uart")
  {
    restartGpsUart("diagnostics page");
    message = String(F("GPS UART restarted at ")) + gpsActiveBaud() + F(" baud.");
  }
  else if (action == "reset_parser")
  {
    resetGpsParser("diagnostics page", true);
    message = String(F("GPS parser reset and UART restarted at ")) + gpsActiveBaud() + F(" baud.");
  }
  else if (action == "clear_raw")
  {
    clearGpsRawNmea();
    ESP_LOGI(TAG, "Cleared retained raw GPS NMEA troubleshooting buffer.");
    message = F("Cleared retained raw GPS NMEA buffer.");
  }
  else
  {
    server.send(400, "application/json; charset=UTF-8", F("{\"ok\":false,\"error\":\"Unknown GPS action.\"}"));
    return;
  }

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=UTF-8",
              String(F("{\"ok\":true,\"message\":\"")) + message + F("\"}"));
}

/** Queues one one-shot display test without changing its normal scheduler timestamps. */
bool queueDisplayTestAction(const String &action, String &message, String &error)
{
  const String normalized = action;

  if (normalized == "current_weather")
  {
    if (!checkweather.complete)
    {
      error = F("Current weather test unavailable. No weather data has been loaded yet.");
      return false;
    }
    showready.testcurrentweather = true;
    message = F("Queued current weather scroller test.");
    return true;
  }

  if (normalized == "daily_weather")
  {
    if (!checkweather.complete)
    {
      error = F("Daily weather test unavailable. No weather data has been loaded yet.");
      return false;
    }
    showready.testdayweather = true;
    message = F("Queued daily weather scroller test.");
    return true;
  }

  if (normalized == "temp_icon")
  {
    if (!checkweather.complete)
    {
      error = F("Temperature and icon test unavailable. No weather data has been loaded yet.");
      return false;
    }
    showready.testcurrenttemp = true;
    message = F("Queued temperature and icon display test.");
    return true;
  }

  if (normalized == "aqi")
  {
    if (!checkaqi.complete)
    {
      error = F("AQI test unavailable. No air quality data has been loaded yet.");
      return false;
    }
    showready.testaqi = true;
    message = F("Queued AQI scroller test.");
    return true;
  }

  if (normalized == "alerts")
  {
    showready.testalerts = true;
    message = alerts.active ? String(F("Queued alert scroller test."))
                            : String(F("Queued alert scroller test using a sample alert."));
    return true;
  }

  error = F("Unknown display test.");
  return false;
}

/** Executes one display test requested from the diagnostics page. */
void handleDisplayTestAction()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeAdminRequest())
    return;

  String action = server.hasArg("action") ? server.arg("action") : String();
  action.trim();
  action.toLowerCase();

  String message;
  String error;
  if (!queueDisplayTestAction(action, message, error))
  {
    server.sendHeader("Cache-Control", "no-store");
    server.send(400, "application/json; charset=UTF-8",
                String(F("{\"ok\":false,\"error\":\"")) + error + F("\"}"));
    return;
  }

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=UTF-8",
              String(F("{\"ok\":true,\"message\":\"")) + message + F("\"}"));
}

/** Renders the live web console page backed by the RAM log buffer. */
void handleConsolePage()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeAdminRequest())
    return;

  String accessToken = issueConsoleAccessToken();
  String html;
  html.reserve(18000);
  appendConsolePage(html, accessToken);
  server.send(200, "text/html; charset=UTF-8", html);
}

/** Streams incremental console output newer than the supplied cursor. */
void handleConsoleLog()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeConsoleRequest())
    return;

  uint32_t since = 0;
  if (server.hasArg("since"))
    since = static_cast<uint32_t>(strtoul(server.arg("since").c_str(), nullptr, 10));

  String payload;
  uint32_t cursor = 0;
  bool truncated = false;
  readConsoleLogSince(since, payload, cursor, truncated);
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("X-Console-Cursor", String(cursor));
  server.sendHeader("X-Console-Truncated", truncated ? "1" : "0");
  server.send(200, "text/plain; charset=UTF-8", payload);
}

/** Downloads the current retained RAM console buffer as plain text. */
void handleConsoleDownload()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeConsoleRequest())
    return;

  String payload;
  uint32_t cursor = 0;
  bool truncated = false;
  readConsoleLogSnapshot(payload, cursor, truncated);
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Content-Disposition", F("attachment; filename=\"ledsmartclock-console.txt\""));
  server.send(200, "text/plain; charset=UTF-8", payload);
}

/** Accepts a one-character debug command from the web console. */
void handleConsoleCommand()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeConsoleRequest())
    return;

  String command = server.hasArg("cmd") ? server.arg("cmd") : String();
  char input = '\0';
  for (size_t index = 0; index < command.length(); ++index)
  {
    if (!isspace(static_cast<unsigned char>(command[index])))
    {
      input = command[index];
      break;
    }
  }

  if (input == '\0')
  {
    server.send(400, "application/json; charset=UTF-8", F("{\"ok\":false,\"error\":\"Missing command.\"}"));
    return;
  }

  ConsoleMirrorPrint out(true);
  out.printf("[web] command: %c\n", input);
  bool handled = handleDebugCommand(input, out, false);
  if (!handled)
  {
    server.send(400, "application/json; charset=UTF-8", F("{\"ok\":false,\"error\":\"Unknown command.\"}"));
    return;
  }

  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=UTF-8", F("{\"ok\":true}"));
}

/** Clears the retained RAM console buffer from the web console page. */
void handleConsoleClear()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeConsoleRequest())
    return;

  clearConsoleLog();
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json; charset=UTF-8",
              String(F("{\"ok\":true,\"cursor\":")) + getConsoleLogCursor() +
                  F(",\"message\":\"Console buffer cleared.\"}"));
}
} // namespace

void configureWebUi()
{
  iotWebConf.setHtmlFormatProvider(&clockHtmlFormatProvider);
}

void registerWebRoutes()
{
  server.collectHeaders(kCollectedHeaderKeys, sizeof(kCollectedHeaderKeys) / sizeof(kCollectedHeaderKeys[0]));
  server.on(kThemeCssPath, HTTP_GET, handleThemeCss);
  server.on(kLoginPath, HTTP_GET, handleLoginPage);
  server.on(kLoginPath, HTTP_POST, handleLoginPost);
  server.on(kLogoutPath, HTTP_GET, handleLogout);
  server.on(kOnboardingPath, HTTP_GET, handleOnboarding);
  server.on(kOnboardingPath, HTTP_POST, handleOnboardingPost);
  server.on(kOnboardingMatrixTestPath, HTTP_GET, handleOnboardingMatrixTest);
  server.on("/", handleRoot);
  server.on(kDiagnosticsPath, []() {
    if (iotWebConf.handleCaptivePortal())
      return;
    if (!authorizeAdminRequest())
      return;

    String html;
    html.reserve(22000);
    if (isLiveRefreshRequest())
    {
      appendLiveShellStart(html, "/diagnostics?partial=1");
      appendDiagnosticsContent(html);
      appendLiveShellEnd(html);
      server.sendHeader("Cache-Control", "no-store");
      server.sendHeader("Pragma", "no-cache");
    }
    else
    {
      appendDiagnosticsPage(html);
    }
    server.send(200, "text/html; charset=UTF-8", html);
  });
  server.on(kDisplayTestPath, HTTP_POST, handleDisplayTestAction);
  server.on(kGpsRawPath, HTTP_GET, handleGpsRaw);
  server.on(kGpsActionPath, HTTP_POST, handleGpsAction);
  server.on(kConsolePath, handleConsolePage);
  server.on(kConsoleLogPath, HTTP_GET, handleConsoleLog);
  server.on(kConsoleDownloadPath, HTTP_GET, handleConsoleDownload);
  server.on(kConsoleClearPath, HTTP_POST, handleConsoleClear);
  server.on(kConsoleCommandPath, HTTP_POST, handleConsoleCommand);
  server.on(kConfigExportPath, HTTP_GET, handleConfigExport);
  server.on(kConfigImportPath, HTTP_GET, handleConfigImport);
  server.on(kConfigImportPath, HTTP_POST, handleConfigImportPost, handleConfigImportUpload);
  server.on(kFirmwareUpdatePath, HTTP_GET, handleFirmwareUpdate);
  server.on(kFirmwareUpdatePath, HTTP_POST, handleFirmwareUpdatePost, handleFirmwareUpload);
  server.on("/config", handleConfigPage);
  server.on("/reboot", handleReboot);
  server.onNotFound([]()
                    {
                      if (isProtectedPortalState() && !hasValidWebSession())
                      {
                        redirectToLogin(server.uri());
                        return;
                      }
                      iotWebConf.handleNotFound();
                    });
}

void handleRoot()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!isSetupPortalState() && !authorizeAdminRequest())
    return;

  String html;
  html.reserve(14000);

  if (isSetupPortalState())
    appendSetupPage(html);
  else if (isLiveRefreshRequest())
  {
    appendLiveShellStart(html, "/?partial=1");
    appendStatusContent(html);
    appendLiveShellEnd(html);
    server.sendHeader("Cache-Control", "no-store");
    server.sendHeader("Pragma", "no-cache");
  }
  else
    appendStatusPage(html);

  server.send(200, "text/html; charset=UTF-8", html);
}

void handleReboot()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  if (!authorizeAdminRequest())
    return;

  runtimeState.rebootRequested = true;
  runtimeState.rebootRequestMillis = millis();

  String html;
  html.reserve(2400);
  appendDocumentHead(html, "LED Smart Clock Reboot", false);
  html += F("<div class='shell'><header class='hero'>");
  html += F("<p class='eyebrow'>LED Smart Clock</p>");
  html += F("<h1>Restarting Device</h1>");
  html += F("<p class='lede'>The clock accepted the reboot request. Give it a few seconds to restart, reconnect to Wi-Fi, and resume the dashboard.</p>");
  html += F("<div class='action-row'><a class='button-link ghost' href='/'>Return to Dashboard</a></div>");
  html += F("</header><section class='notice notice-warn'>If the page stops responding during the restart window, refresh the dashboard after the device finishes booting.</section></div></body></html>");
  server.send(200, "text/html; charset=UTF-8", html);
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
