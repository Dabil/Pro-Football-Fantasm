# Pro Football Fantasm — Repository Instructions

## Scope and workspace boundaries

This file belongs at the root of the single Pro Football Fantasm Git repository, alongside `PFF/` and `Vendor/`. It governs development throughout this repository.

```text
<repository root>/
  AGENTS.md
  PFF/
  Vendor/
    TUI/
    GCE/
```

- `PFF/` contains the application; `Vendor/TUI/` and `Vendor/GCE/` contain its local framework copies. All are ordinary tracked directories in one Git repository, not submodules or independent repositories.
- The user's independent original TUI and GCE repositories are outside this workspace and are strictly out of scope. Never open, inspect, modify, build, synchronize, or otherwise operate on those originals during PFF work. Do not add them to this workspace.
- Read and edit project files only within this repository for the authorized task. Standard installed toolchains may be used; this does not authorize changes to external projects or machine configuration.
- Before writing or running project scripts, verify the repository root and resolved target paths. Check relevant symbolic links, junctions, reparse points, and build destinations. Do not follow them into the original projects or write outside this repository. If the actual layout contradicts the stated single-repository model, report it and stop affected work; do not restructure it automatically.
- Do not convert vendor directories to submodules, nested repositories, external checkout dependencies, or package downloads.

## Instructions and authorization

- Follow applicable platform instructions and the user's current explicit instructions. This file does not grant permission to begin development simply because a repository is open.
- Read applicable nested `AGENTS.md` and `AGENTS.override.md` guidance before editing their areas. Codex can apply more specific instructions and same-directory overrides; do not assume this root file is the only active guidance. Surface conflicts affecting scope, originals, specifications, or approval before proceeding. Do not create overrides to bypass these rules.
- Discussion, review requests, and requests for recommendations authorize analysis, not implementation. A clear request to implement a specific bounded change authorizes that work within these rules. Do not repeatedly request approval already given for the same scope.
- Obtain approval before expanding scope, making material architectural or specification changes, changing public framework contracts, adding dependencies, changing required toolchains, migrating data, or altering the repository/build layout.
- Approval to implement is separate from approval to change specifications, accept the result, or perform Git publication/history operations. Silence, a proposed plan, and a passing build are not approval.
- Do not change these instructions unless the user requests or approves the change.

## Authoritative specifications and access

- The current user-approved specifications in the ChatGPT project's **Project Sources** are the authoritative product and design baseline for PFF, TUI, and GCE. Use the applicable approved versions, not whichever copy has the newest timestamp.
- Repository documentation, accessible exports, and attachments can serve as working copies only when their identity, revision, approval status, and relationship to the authoritative sources are established. An approved user change may supersede part of a specification; record the exact decision and affected sections rather than silently discarding the baseline.
- Treat implementation, old conversations, summaries, generated documents, and suggestions as evidence or proposals, not automatic replacements for approved specifications. Do not infer new requirements merely from existing code.
- ChatGPT Project Sources may not be automatically accessible from Codex. Never imply that a document was read or checked unless its contents were actually available and reviewed. A filename, link, summary, or mention is not verification of its contents or currency.
- At task start, identify the applicable specifications by title, version/revision, accessible location, and relevant section. State which sources were read, which are missing, and whether their currency or approval could be verified.
- If required specifications are inaccessible, incomplete, ambiguous, or conflicting, explain the gap and request the relevant documents or an explicit user decision. Continue independent read-only investigation where useful, but pause implementation whose correctness depends on that gap. Do not invent requirements or claim full compliance.
- Do not invent a local documentation directory or claim one exists. Discover the existing convention first. If local specification exports are needed, propose a location and provenance/index convention for approval. Do not automatically upload, download, replace, or synchronize Project Sources and local copies.

## Review and approval workflow

1. **Inspect:** Confirm task scope, repository boundaries, current branch, working-tree status, applicable instructions, available specifications, and relevant solution/project structure. Preserve existing user work. For a newly connected or unfamiliar workspace, begin with a read-only architectural assessment before proposing material changes.
2. **Review across specifications:** Read the directly applicable specifications and related contracts across PFF, TUI, and GCE. Identify affected interfaces, ownership, data formats, lifecycle, error handling, UI behavior, simulation behavior, compatibility, and build/runtime assumptions as relevant. Check dependent and referencing specifications, not just the document named in the request.
3. **Propose when needed:** For material or ambiguous work, present the problem, evidence, proposed behavior, affected code and documents, alternatives/tradeoffs, compatibility risks, and validation plan. Distinguish required corrections from optional improvements. Obtain approval for unresolved design choices or scope expansion before editing affected implementation.
4. **Implement within approval:** Make focused changes consistent with the approved requirements. Necessary routine edits and tests within approved scope can proceed. If a new conflict or material design issue emerges, stop the affected part, explain it, and seek a decision while continuing independent authorized work.
5. **Verify and document:** Review the final diff, run relevant available checks, update authorized change records, and report remaining risks and specification impacts. Present results for user review. Do not mark a proposal or unreviewed document as approved.

### Specification changes

- Do not make speculative material edits to approved specifications or rewrite them to justify code that already changed.
- Before a material specification change, provide the current rule and source section, proposed wording or reviewable diff, rationale, alternatives, and cross-spec impact. Obtain explicit approval before updating the authoritative baseline or implementing behavior that depends on that change.
- Approval to draft a proposal does not approve its adoption. Keep drafts clearly labeled and separate from approved versions.
- Review all related specifications for conflicts, duplication, terminology, dependencies, and consequences. Report affected documents even when they need no edit. If the related set cannot be fully reviewed, name the gap and qualify the result; never claim a complete cross-spec review.
- Preserve unrelated content, stable identifiers, historical decisions, and the existing document/versioning conventions. Do not broadly reorganize specifications as part of a narrow change.

## Versioned documentation, changelogs, and provenance

- Use the repository's established versioned changelog and documentation conventions after verifying them. If none exist, propose a minimal convention and obtain approval before introducing a new documentation system. No particular changelog filename or specification path is assumed here.
- For each approved specification revision, retain its document identity, prior and new revision, date, status, exact changed sections, rationale, approval reference, related specifications, and implementation implications. Preserve previous history; do not silently replace a version or invent an approval/date/version.
- For implementation changes, record the task/decision reference, affected components, behavior and compatibility changes, applicable specification revisions, and validation results. Keep pending/unreleased work distinct from released versions; do not invent release numbers.
- For local source copies or exports, record the original source identity/location, revision, export/retrieval date when known, approval status, and any local divergence. Clearly distinguish verified metadata from unknown or user-reported information.
- Keep approved specification changes, corresponding working copies, and implementation records consistent within authorized scope. If publication to Project Sources is inaccessible or unauthorized, provide the proposed update and explicitly report that publication remains pending.
- Never claim a document was saved, synchronized, published, approved, or verified without evidence. A generated draft is not an authoritative update.

## Vendor development and eventual upstream transfer

- Changes under `Vendor/TUI/` and `Vendor/GCE/` are permitted when necessary for an approved PFF development task. Keep changes focused and preserve framework responsibilities; avoid introducing PFF-specific concepts into reusable framework APIs without an approved design decision.
- Maintain a durable vendor-change record using an existing repository convention, or propose a minimal record location if none exists. Include component, change identifier, affected files/symbols, motivation, specification/approval reference, behavior/API/compatibility impact, tests and outcomes, and dependencies on other local changes.
- Record the imported upstream version/commit/source only if known and verified from in-repository evidence or supplied provenance. Mark unknown baselines as unknown; never inspect the originals to fill this gap during PFF work.
- Classify each vendor change as PFF-specific, a potential general-purpose upstream candidate, or undecided. Record adaptation needs and dependencies so changes can later be selected individually. This classification is a recommendation, not transfer approval.
- Preserve license, copyright, and attribution notices. Do not replace whole vendor trees to simplify a small update.
- Do not synchronize with upstream, copy changes to the original repositories, or assume automatic upstream acceptance. Later transfer requires a separate explicitly authorized task in the appropriate original-project workspace, outside this PFF task. Because these are tracked vendor directories, do not assume they have separate commit histories suitable for direct cherry-picking.

## Preserve the Visual Studio 2022 F5 experience

- Preserve the intended workflow: obtain the repository, open the existing Visual Studio 2022 solution, and press F5 to build and run with the project's documented standard prerequisites. Keep all required vendored source in this repository.
- Discover the actual solution filename, startup project, supported configurations/platforms, toolset, dependencies, asset/data locations, working directory, and debugger settings. Do not guess solution paths or build commands.
- Preserve relative project references and portable resource paths. Do not introduce developer-specific absolute paths, references to independent original repositories, hidden machine setup, extra cloning steps, or undocumented manual copying.
- Preserve the existing VS2022 toolchain and configuration unless a change is explicitly approved. Do not automatically upgrade project formats, SDKs, language standards, package versions, or build systems.
- When approved work affects build or runtime setup, verify the relevant configurations and document any approved prerequisite changes. A command-line build alone does not prove F5 startup and runtime behavior.

## Code and repository safety

- Inspect the existing code and follow its naming, formatting, ownership, error-handling, and architectural conventions. Prefer the smallest coherent change that satisfies the approved task. Avoid unrelated refactors, cleanup, mass formatting, or broad line-ending changes.
- Preserve uncommitted and staged user changes. Do not revert, overwrite, stage, or discard unrelated work. If concurrent edits overlap, stop that edit and resolve the overlap with the user.
- Do not commit, amend, push, merge, rebase, cherry-pick, pull, fetch/synchronize, publish, create tags, or change branches/worktrees without explicit authorization for the operation and scope. Read-only Git inspection is allowed. Do not stage changes unless requested.
- Do not reset, clean, delete tracked/untracked files, rewrite history, alter remotes, initialize nested repositories, or change global Git settings as a convenience. Destructive operations require specific approval.
- Before running scripts, inspect relevant side effects and output destinations. Keep project-generated outputs inside the repository and avoid modifying real user data during tests. Do not run automation that touches external original projects or performs prohibited Git operations.
- Do not expose credentials or private data in code, logs, reports, or commits. Do not add secrets, machine-specific IDE state, generated build outputs, or unrelated files to tracked deliverables.

## Testing and completion

- Discover existing build/test instructions and available tools before selecting checks. Use actual commands and targets from the repository/toolchain; do not invent tests or assume a test suite exists.
- Run focused checks appropriate to changed behavior, then relevant integration/regression checks. For vendor changes, validate both the affected framework behavior and its PFF integration. Add or adjust meaningful tests where warranted by behavior and existing test conventions.
- Use isolated fixtures/copies for imports, saves, migrations, and other data-changing checks. Do not modify genuine user databases or saves without explicit authorization.
- Compare failures with the initial state when practical. Report pre-existing failures separately from new regressions; do not silently fix unrelated problems.
- Verify F5 startup in VS2022 when available and relevant, including runtime data/assets and affected user flows. If unavailable, state that manual VS2022/F5 verification remains outstanding and provide concise steps using verified project details.
- Record exactly what ran, the configuration/environment, outcomes, and anything skipped or blocked. Distinguish code inspection, successful compilation, automated tests, and observed runtime behavior. Never report an unrun check as passed.
- Before finishing, review the diff and status for unintended changes, external path dependencies, specification drift, and missing vendor/documentation records.
- The completion report should summarize changed behavior and files, governing specifications and approval decisions, cross-spec findings, vendor implications, checks/results, and outstanding issues. State whether documentation publication or user acceptance remains pending. Completion of editing does not authorize Git operations.

## Code Review Rules

Flag actionable defects with file/line evidence and their practical impact. In particular, check for:

- Paths, scripts, or project references that reach outside the repository or into original TUI/GCE projects.
- Breakage of the VS2022 build/F5 workflow or introduction of hidden setup requirements.
- Behavior inconsistent with an identified approved specification, unapproved material contract changes, or conflicts across PFF/TUI/GCE specifications.
- Vendor changes missing traceable rationale, compatibility analysis, validation, or a durable change record.
- Destructive data handling, lost user changes, unauthorized Git operations, and unsupported claims of verification or approval.

Distinguish verified defects from questions caused by missing specifications or unavailable execution environments. Do not present assumptions as established requirements.
