"""
Google Calendar monitor — fetches today's events for the calendar panel.

Uses the Google Calendar API with the read-only token saved by gcal_auth.py.
Every CALENDAR_POLL_INTERVAL (and right after local midnight) it pulls today's
events from each calendar listed in CALENDAR_IDS_FILE, drops cancelled, declined, and working-location
events, merges duplicates (the same meeting on both calendars), and keeps the
result for main.py to send: timed events as [[start_min, end_min, title], ...],
all-day events (including each day of a multi-day one) as a list of titles.

If the token is missing or a fetch fails, the last good list is kept; get()
returns None only when nothing has ever been fetched, so the firmware can tell
"no calendar" apart from "no meetings today".

Run directly to list every calendar the logged-in account can see.
"""

import threading
import time
from datetime import date, datetime, timedelta

from config import (CALENDAR_IDS_FILE, CALENDAR_POLL_INTERVAL, GOOGLE_SCOPES,
                    GOOGLE_TOKEN_FILE)
from text_utils import fit_utf8, to_displayable

_API = "https://www.googleapis.com/calendar/v3"

MAX_EVENTS      = 12   # must not exceed CAL_MAX_EVENTS in the firmware's config.h
MAX_ALLDAY      = 6    # must not exceed CAL_MAX_ALLDAY in the firmware's config.h
MAX_TITLE_BYTES = 39   # firmware title buffer is 40 bytes incl. terminator


def _session():
    from google.auth.transport.requests import AuthorizedSession
    from google.oauth2.credentials import Credentials
    creds = Credentials.from_authorized_user_file(str(GOOGLE_TOKEN_FILE), GOOGLE_SCOPES)
    return AuthorizedSession(creds)  # refreshes the access token on its own


def _calendar_ids() -> list[str]:
    """Read on every fetch, so edits apply without restarting the companion."""
    try:
        ids = [l.split("#", 1)[0].strip() for l in CALENDAR_IDS_FILE.read_text().splitlines()]
        return [i for i in ids if i] or ["primary"]
    except OSError:
        return ["primary"]


def _minutes(dt: datetime, day_start: datetime) -> int:
    """Minutes since local midnight, clamped to today (events may span midnight)."""
    return max(0, min(24 * 60, int((dt - day_start).total_seconds() // 60)))


class CalendarMonitor:
    def __init__(self):
        self._lock    = threading.Lock()
        self._events  = None   # list of [start_min, end_min, title], or None
        self._allday  = []     # titles of today's all-day events
        self._day     = None   # date the list belongs to
        self._running = False
        self._enabled = GOOGLE_TOKEN_FILE.exists()
        if not self._enabled:
            print("  [calendar] no Google token — run gcal_auth.py to enable the calendar panel")

    def start(self):
        if not self._enabled:
            return
        self._running = True
        threading.Thread(target=self._run, daemon=True).start()

    def stop(self):
        self._running = False

    def get(self) -> list | None:
        with self._lock:
            if self._events is None:
                return None
            # Yesterday's list is stale past midnight even if a refetch failed
            return self._events if self._day == date.today() else []

    def get_allday(self) -> list:
        with self._lock:
            return self._allday if self._day == date.today() else []

    # ------------------------------------------------------------------
    def _run(self):
        session, last = None, 0.0
        while self._running:
            if time.monotonic() - last >= CALENDAR_POLL_INTERVAL or self._day != date.today():
                last = time.monotonic()
                try:
                    session = session or _session()
                    self._fetch(session)
                except Exception as e:
                    print(f"  [calendar] fetch error: {e}")
            time.sleep(15)

    def _fetch(self, session):
        today     = date.today()
        day_start = datetime.combine(today, datetime.min.time()).astimezone()
        params = {
            "timeMin":      day_start.isoformat(),
            "timeMax":      (day_start + timedelta(days=1)).isoformat(),
            "singleEvents": "true",   # expand recurring meetings
            "orderBy":      "startTime",
            "maxResults":   "100",
        }

        seen, events, allday = set(), [], []
        for cal_id in _calendar_ids():
            r = session.get(f"{_API}/calendars/{cal_id}/events", params=params, timeout=15)
            r.raise_for_status()
            for ev in r.json().get("items", []):
                if ev.get("status") == "cancelled":
                    continue
                if ev.get("eventType") == "workingLocation":
                    continue  # "Office" / "Home" markers, not events
                if any(a.get("self") and a.get("responseStatus") == "declined"
                       for a in ev.get("attendees", [])):
                    continue
                start, end = ev.get("start", {}), ev.get("end", {})
                title = fit_utf8(to_displayable(ev.get("summary", "(no title)").strip()),
                                 MAX_TITLE_BYTES)
                if "date" in start and "date" in end:
                    # All-day; the end date is exclusive
                    if (date.fromisoformat(start["date"]) <= today < date.fromisoformat(end["date"])
                            and title not in allday):
                        allday.append(title)
                    continue
                if "dateTime" not in start or "dateTime" not in end:
                    continue
                s = _minutes(datetime.fromisoformat(start["dateTime"]), day_start)
                e = _minutes(datetime.fromisoformat(end["dateTime"]), day_start)
                if e <= s:
                    continue
                key = (s, e, title)
                if key in seen:
                    continue  # same meeting on two calendars
                seen.add(key)
                events.append([s, e, title])

        events.sort()
        with self._lock:
            self._events = events[:MAX_EVENTS]
            self._allday = allday[:MAX_ALLDAY]
            self._day    = today


if __name__ == "__main__":
    r = _session().get(f"{_API}/users/me/calendarList", timeout=15)
    r.raise_for_status()
    for c in r.json().get("items", []):
        print(f"{c['id']:60}  {c.get('summaryOverride') or c.get('summary')}")
