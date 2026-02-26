# Security Policy

## Supported Versions

Archura Game Engine is currently in an alpha stage and constantly evolving. As such, only the most recent minor versions and the `main` branch are officially supported with security updates.

| Version | Supported          |
| ------- | ------------------ |
| `main`  | :white_check_mark: |
| 1.0.x   | :white_check_mark: |
| < 1.0   | :x:                |

*(Note: Version numbers will be updated as the engine stabilizes towards a formal release).*

## Reporting a Vulnerability

We take the security of Archura Engine (especially its new networking and dedicated server architectures) very seriously. 

If you discover a security vulnerability, please **DO NOT** open a public issue. 

Instead, please report it privately:
1.  **Email:** Send an email detailing the vulnerability to the repository owner/lead maintainer (include your contact email here, e.g., `[Your Email Address]`).
2.  **GitHub Security Advisories:** Alternatively, you can privately report the vulnerability via the "Security" tab on our GitHub repository. 

**What to expect:**
*   You should receive an acknowledgment of your report within 48 hours.
*   We will immediately assess the severity and impact of the reported vulnerability, particularly regarding how it affects the engine's network (`src/network/`) and server (`src/server/`) modules.
*   If accepted, we will work on a patch and notify you before it is publicly released.
*   We will happily credit you (if you desire) in the release notes for finding and responsibly disclosing the vulnerability.

Please provide detailed instructions on how to reproduce the issue, including scripts, logs (`crash_log.txt`), or network packet traces if the vulnerability involves the `ServerConfig` or `NetworkManager`.
