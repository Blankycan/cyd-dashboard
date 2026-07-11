"""
Claude usage monitor — two data sources:

  1. JSONL scan (always): reads ~/.claude/projects/**/*.jsonl for today's
     output/input token counts and session count.  Runs every 60 s.

  2. Rate-limit fetch: makes a minimal Haiku API call (~8 input tokens) to
     read the 5h and 7d unified rate-limit headers.  Runs every 300 s.

     Auth priority:
       a) OAuth token from ~/.claude/.credentials.json  (Claude Code login —
          no setup required; refreshed automatically via platform.claude.com)
       b) ANTHROPIC_API_KEY environment variable (fallback)
     If neither is available the rate-limit fields stay at -1.
"""
import glob
import http.client
import json
import os
import threading
import time
from datetime import date, datetime
from pathlib import Path

from config import SCAN_INTERVAL, FETCH_INTERVAL

_CREDENTIALS_FILE  = Path.home() / ".claude" / ".credentials.json"
_TOKEN_CACHE_FILE  = Path.home() / ".claude" / "proxy_oauth_token.json"
_OAUTH_CLIENT_ID   = "9d1c250a-e61b-44d9-88ed-5944d1962f5e"
_OAUTH_SCOPES      = ("user:profile user:inference user:sessions:claude_code "
                      "user:mcp_servers user:file_upload")


# ---------------------------------------------------------------------------
# OAuth helpers
# ---------------------------------------------------------------------------

def _load_credentials() -> dict | None:
    """Return {accessToken, refreshToken, expiresAt} or None."""
    # 1. Try cached token written by a previous refresh
    try:
        d = json.loads(_TOKEN_CACHE_FILE.read_text())
        if d.get("accessToken") and d.get("expiresAt"):
            return d
    except Exception:
        pass
    # 2. Try Claude Code's own credentials file
    try:
        d = json.loads(_CREDENTIALS_FILE.read_text())
        tok = d.get("claudeAiOauth", {})
        if tok.get("accessToken"):
            return tok
    except Exception:
        pass
    return None


def _save_credentials(tok: dict) -> None:
    try:
        _TOKEN_CACHE_FILE.write_text(json.dumps(tok))
    except Exception:
        pass


def _refresh(refresh_token: str) -> dict | None:
    """Exchange a refresh token for a new access token."""
    body = json.dumps({
        "grant_type":    "refresh_token",
        "refresh_token": refresh_token,
        "client_id":     _OAUTH_CLIENT_ID,
        "scope":         _OAUTH_SCOPES,
    }).encode()
    conn = None
    try:
        conn = http.client.HTTPSConnection("platform.claude.com", timeout=15)
        conn.request("POST", "/v1/oauth/token", body=body,
                     headers={"content-type": "application/json",
                              "content-length": str(len(body))})
        resp = conn.getresponse()
        data = json.loads(resp.read())
        if resp.status != 200 or "access_token" not in data:
            return None
        return {
            "accessToken":  data["access_token"],
            "refreshToken": data.get("refresh_token", refresh_token),
            "expiresAt":    int(time.time() * 1000) + data["expires_in"] * 1000,
        }
    except Exception:
        return None
    finally:
        if conn:
            try: conn.close()
            except: pass


def _get_valid_token() -> str | None:
    """Return a valid access token, refreshing if needed."""
    tok = _load_credentials()
    if not tok:
        return None
    # expiresAt is in milliseconds; refresh 60 s early
    if tok.get("expiresAt", 0) <= (time.time() + 60) * 1000:
        refreshed = _refresh(tok.get("refreshToken", ""))
        if refreshed:
            _save_credentials(refreshed)
            tok = refreshed
        else:
            return None  # can't refresh
    return tok.get("accessToken")


# ---------------------------------------------------------------------------
# Monitor class
# ---------------------------------------------------------------------------

class ClaudeTokenMonitor:
    def __init__(self):
        self._lock    = threading.Lock()
        self._data    = {
            "out": 0, "inp": 0, "sessions": 0,
            "h5_pct": -1, "h5_secs": -1,
            "w7_pct": -1, "w7_secs": -1,
        }
        self._running  = False
        self._have_auth = bool(_load_credentials() or
                               os.environ.get("ANTHROPIC_API_KEY"))
        if not self._have_auth:
            print("  [claude] no auth found — rate-limit view disabled")

    def start(self):
        self._scan()
        if self._have_auth:
            self._fetch()
        self._running = True
        threading.Thread(target=self._scan_loop,  daemon=True).start()
        if self._have_auth:
            threading.Thread(target=self._fetch_loop, daemon=True).start()

    def stop(self):
        self._running = False

    def get(self) -> dict:
        with self._lock:
            return dict(self._data)

    # ------------------------------------------------------------------
    def _scan_loop(self):
        while self._running:
            time.sleep(SCAN_INTERVAL)
            self._scan()

    def _fetch_loop(self):
        while self._running:
            time.sleep(FETCH_INTERVAL)
            self._fetch()

    # ------------------------------------------------------------------
    def _scan(self):
        today       = date.today()          # local date
        today_start = time.mktime(today.timetuple())
        pattern     = str(Path.home() / ".claude" / "projects" / "**" / "*.jsonl")
        candidates  = [f for f in glob.glob(pattern, recursive=True)
                       if os.path.getmtime(f) >= today_start]

        out_today = 0
        inp_today = 0
        sessions: set[str] = set()

        for fpath in candidates:
            try:
                active = False
                with open(fpath, errors="replace") as f:
                    for line in f:
                        try:
                            d = json.loads(line)
                            if d.get("type") != "assistant":
                                continue
                            # Timestamps are UTC — convert to local before comparing
                            ts = d.get("timestamp", "")
                            if not ts:
                                continue
                            try:
                                dt = datetime.fromisoformat(ts.replace("Z", "+00:00"))
                                if dt.astimezone().date() != today:
                                    continue
                            except (ValueError, TypeError):
                                continue
                            u = d.get("message", {}).get("usage", {})
                            out_today += u.get("output_tokens", 0)
                            inp_today += (u.get("input_tokens", 0)
                                        + u.get("cache_creation_input_tokens", 0)
                                        + u.get("cache_read_input_tokens", 0))
                            active = True
                        except Exception:
                            pass
                if active:
                    sessions.add(fpath)
            except Exception:
                pass

        with self._lock:
            self._data["out"]      = out_today
            self._data["inp"]      = inp_today
            self._data["sessions"] = len(sessions)

    # ------------------------------------------------------------------
    def _fetch(self):
        # Prefer OAuth token; fall back to API key
        access_token = _get_valid_token()
        api_key      = os.environ.get("ANTHROPIC_API_KEY", "")

        if access_token:
            auth_headers = {"Authorization": f"Bearer {access_token}"}
        elif api_key:
            auth_headers = {"x-api-key": api_key}
        else:
            return

        body = json.dumps({
            "model":      "claude-haiku-4-5-20251001",
            "max_tokens": 1,
            "messages":   [{"role": "user", "content": "."}],
        }).encode()

        conn = None
        try:
            conn = http.client.HTTPSConnection("api.anthropic.com", timeout=15)
            conn.request(
                "POST", "/v1/messages",
                body=body,
                headers={
                    **auth_headers,
                    "anthropic-version": "2023-06-01",
                    "content-type":      "application/json",
                    "content-length":    str(len(body)),
                },
            )
            resp = conn.getresponse()
            resp.read()
            h   = {k.lower(): v for k, v in resp.getheaders()}
            now = time.time()

            def pct(key):
                try:  return round(float(h[key]) * 100)
                except: return -1

            def secs(key):
                try:  return max(0, int(float(h[key])) - int(now))
                except: return -1

            with self._lock:
                self._data["h5_pct"]  = pct("anthropic-ratelimit-unified-5h-utilization")
                self._data["h5_secs"] = secs("anthropic-ratelimit-unified-5h-reset")
                self._data["w7_pct"]  = pct("anthropic-ratelimit-unified-7d-utilization")
                self._data["w7_secs"] = secs("anthropic-ratelimit-unified-7d-reset")
        except Exception as e:
            print(f"  [claude] fetch error: {e}")
        finally:
            if conn:
                try: conn.close()
                except: pass
