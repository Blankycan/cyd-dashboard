#!/usr/bin/env python3
"""
One-time Google Calendar login for the CYD Dashboard companion.

Opens a browser tab asking for read-only calendar access, then saves a
refresh token to GOOGLE_TOKEN_FILE. The companion renews access from that
token on its own, so this only needs re-running if the token is revoked.

Run: python companion/gcal_auth.py
"""
import os

from google_auth_oauthlib.flow import InstalledAppFlow

from config import GOOGLE_CLIENT_FILE, GOOGLE_SCOPES, GOOGLE_TOKEN_FILE


def main():
    if not GOOGLE_CLIENT_FILE.exists():
        raise SystemExit(f"Missing OAuth client file: {GOOGLE_CLIENT_FILE}")

    flow  = InstalledAppFlow.from_client_secrets_file(str(GOOGLE_CLIENT_FILE), GOOGLE_SCOPES)
    creds = flow.run_local_server(port=0, open_browser=True,
                                  authorization_prompt_message="Opening browser for Google login: {url}")

    # Owner-only, since the refresh token grants ongoing calendar read access.
    fd = os.open(GOOGLE_TOKEN_FILE, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w") as f:
        f.write(creds.to_json())
    print(f"Saved token to {GOOGLE_TOKEN_FILE}")


if __name__ == "__main__":
    main()
