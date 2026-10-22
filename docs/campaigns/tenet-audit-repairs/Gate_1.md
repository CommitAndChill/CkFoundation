# Gate 1: integrity at ownership, test results, and storage

## Entry

- Record source revisions and existing dirty ownership in PROGRESS.md.
- Read the relevant production paths, failure contracts, and existing test patterns.
- Preserve the maintainer-approved invalid-fragment fallback; the API-migration finding was withdrawn.
- Storage-provider explicit outcomes and local caller migration are approved, including reflected signatures.

## Work and verification

- Add focused regressions using real ECS worlds, production bridge processing, and real file denial
  -> verify the current behavior fails assertions without an unbounded test or unsafe external mutation.
- Validate proposed ownership edges before mutation and enforce same-registry reverse links
  -> verify invalid/stale/self/descendant owners leave both sides unchanged; valid transfers work.
- Make bridge infrastructure failure explicit and publish results before consuming requests
  -> verify missing infrastructure cannot report success and failed I/O cannot erase the verdict/request.
- Distinguish missing settings storage from failed reads and propagate acceptance
  -> verify unreadable existing bytes survive; recovery preserves unrelated keys; rejection causes no publication.
- Review every changed failure predicate/body, normalize formatting, build and run compatible focused tests
  -> verify compiler result, test totals, fresh startup errors/ensures, and source ownership.

## Exit

Each item has source and runtime evidence in PROGRESS.md, or an explicit unresolved decision/blocker.
No broad regression-free claim follows from a focused gate. Permanent module contracts describe changes.
