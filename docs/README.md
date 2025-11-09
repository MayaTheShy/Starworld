# Starworld Documentation

This directory contains all project documentation organized by topic.

## Documentation Index

### Getting Started
- **[../README.md](../README.md)** - Main project README with quick start guide
- **[DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md)** - Developer quick reference with commands and tips
- **[DIRECTORY_STRUCTURE.md](DIRECTORY_STRUCTURE.md)** - Project organization and file layout

### Project Information
- **[CHANGELOG.md](CHANGELOG.md)** - Version history and changes
- **[CI_SETUP_SUMMARY.md](CI_SETUP_SUMMARY.md)** - Continuous integration setup and workflow

### Implementation Guides
- **[OVERTE_ASSIGNMENT_CLIENT_TASK.md](OVERTE_ASSIGNMENT_CLIENT_TASK.md)** - Overte protocol implementation details
- **[OVERTE_AUTH.md](OVERTE_AUTH.md)** - OAuth 2.0 authentication implementation guide
- **[ENTITY_RENDERING_ENHANCEMENTS.md](ENTITY_RENDERING_ENHANCEMENTS.md)** - Entity rendering system
- **[MODELCACHE_IMPLEMENTATION.md](MODELCACHE_IMPLEMENTATION.md)** - Asset download and caching

### Planning Documents
- **[CODE_CLEANUP_PLAN.md](CODE_CLEANUP_PLAN.md)** - Code organization and cleanup plans

## Quick Links

### For New Developers
1. Start with [../README.md](../README.md) for project overview
2. Read [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md) for build/run commands
3. Check [CHANGELOG.md](CHANGELOG.md) to understand recent changes

### For Contributors
1. Review [CI_SETUP_SUMMARY.md](CI_SETUP_SUMMARY.md) for testing workflow
2. Read relevant implementation guides for the area you're working on
3. Follow conventions described in [CODE_CLEANUP_PLAN.md](CODE_CLEANUP_PLAN.md)

### For Protocol Work
1. Study [OVERTE_ASSIGNMENT_CLIENT_TASK.md](OVERTE_ASSIGNMENT_CLIENT_TASK.md) for protocol details
2. Review [OVERTE_AUTH.md](OVERTE_AUTH.md) if working on authentication
3. Check Overte source code references in each document

### For Rendering Work
1. Read [ENTITY_RENDERING_ENHANCEMENTS.md](ENTITY_RENDERING_ENHANCEMENTS.md) for rendering pipeline
2. Review [MODELCACHE_IMPLEMENTATION.md](MODELCACHE_IMPLEMENTATION.md) for asset loading
3. Check the Rust bridge code in `../bridge/src/lib.rs`

## External Resources

### StardustXR
- Website: https://stardustxr.org
- Documentation: https://stardustxr.org/docs
- GitHub: https://github.com/StardustXR

### Overte
- Website: https://overte.org
- Documentation: https://docs.overte.org
- GitHub: https://github.com/overte-org/overte
- Discord: https://discord.gg/overte

## Contributing to Documentation

When adding or updating documentation:
1. Keep documents focused on a single topic
2. Use clear, descriptive filenames
3. Include code examples where helpful
4. Update this README.md index
5. Link between related documents
6. Keep the main README.md up to date with links

### Documentation Style
- Use Markdown format (`.md`)
- Include a clear title and overview
- Use code blocks with language tags
- Add links to external resources
- Keep line length reasonable for readability
- Use relative links for internal references
