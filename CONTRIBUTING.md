# Contributing to Prakshep Optimization Engine

First off, thank you for considering contributing to the Prakshep Optimization Engine! It's people like you that make Prakshep such a great tool for the aerospace community.

## Code of Conduct

By participating in this project, you are expected to uphold our [Code of Conduct](CODE_OF_CONDUCT.md).

## How Can I Contribute?

### Reporting Bugs
If you find a bug in the source code or a mistake in the documentation, you can help us by submitting an issue to our GitHub Repository. Even better, you can submit a Pull Request with a fix.

### Suggesting Enhancements
If you have a feature request or an enhancement idea, please submit an issue. We label these as `enhancement` and discuss them openly.

### Open Issues
Check our `.github/ISSUES.md` or our GitHub Issues tab for a list of open tasks. We tag easy issues as `good first issue` and critical tasks as `critical`. 

## Development Process

1. **Fork the repo** and create your branch from `main`.
2. **If you've added code** that should be tested, add tests.
3. **If you've changed APIs**, update the documentation.
4. **Ensure the test suite passes**.
5. **Issue that pull request!**

### C++ Engine Contributions
If you are contributing to the `cpp_engine`, ensure you:
- Compile using standard CMake bindings.
- Do not add massive third-party dependencies unless discussed first.
- Keep the physics loops tight.

### Next.js / WebGPU Contributions
If you are touching the `webgpu_client`:
- Do not run physics iterators in the main JS thread.
- Ensure the TypeScript compiler passes (`npm run build`).
- Keep 3D assets optimized (use low-poly `.glb` or procedural geometry where possible).
