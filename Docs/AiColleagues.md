# AI colleagues (optional, off by default)

Owner-approved exception to the offline rule (2026-10-21): players may talk to Valtren's people in their own words.

- **Off by default.** Nothing is sent anywhere unless the player turns on *AI colleagues* in Settings and pastes their own Anthropic API key. With it off, the game is fully offline and unchanged.
- **Key storage.** `Saved/Config/RiskboundAi.cfg` on the player's PC only. Never in the save file, never logged; logs record only the HTTP status.
- **Where.** Journal > People > *Talk* next to a colleague (work contacts only).
- **What is sent.** An in-character system prompt (`rb::ColleagueSystemPrompt`, RbAiPrompt): the person's role and bio, how they feel about the player, Valtren's week, reputation and morale, and the active case's title and briefing; plus the last 12 turns of that chat. No save data beyond that, no personal data.
- **Request.** `POST https://api.anthropic.com/v1/messages`, model `claude-opus-5-5`, `max_tokens` 1024, `output_config.effort` low, server-side refusal fallbacks (`fallbacks: "default"`, beta `server-side-fallback-2026-07-01`). Cost is roughly one US cent per reply at current prices.
- **Guardrails in the prompt.** Stay in character, short spoken replies, never give case answers, scores or figures to work out, fictional names only.
- **Failures.** Bad key (401), rate limits (429), service errors (5xx), no connection and refusals all show a short message in the chat; the rest of the game is unaffected.
- **Tests.** `Ai_PromptRequestAndResponseParsing` (request JSON validity, response and error parsing). The live service is not called by tests.
