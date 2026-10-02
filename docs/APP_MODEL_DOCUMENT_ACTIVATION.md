# App Model document activation

Developer Studio accepts owned App Model document activations under the
existing application identity `com.guidexos.developerstudio`. The current
manifest capabilities are `.c`, `.cc`, `.cpp`, `.cxx`, `.h`, `.hh`, `.hpp`,
`.hxx`, and `.txt`.

At process startup, Developer Studio asks the Native App host ABI for a bounded
copy of the activation path. It rejects paths that do not match its manifest
extension set and opens supported paths with
`WorkspaceControllerOpenStandaloneDocument`. That creates a projectless
workspace at the containing directory and reuses the regular
`WorkspaceControllerOpenDocument` loader. It does not create project metadata
or a second editor buffer/dirty-state implementation.

The workspace path limit is 768 bytes, the hosted filesystem path limit is 240
bytes, and the existing per-document editor limit is 256 KiB. The loader keeps
file bytes and newline sequences unchanged; it does not promise arbitrary text
encoding support and rejects binary-looking or over-limit files. A missing
file reports the ordinary loader failure. Save and dirty-close behavior use the
existing document path and controller. The current application has no Save As
operation.

Each launch has its own NativeElf process context. Existing tabs remain owned
by their workspace/controller, and separate launches do not share editor
buffers or cursor/dirty state. Project actions remain unavailable when no
project is open.

The Server's normal `build.bat` intentionally leaves its experimental NativeElf
executor disabled, so Developer Studio is not advertised as an available
document handler in that runtime. For hosted activation validation, use
`build-native-experimental.bat` after building the package. The end-to-end
route, limitations, test results, and SDK compatibility note are recorded in
the Server [Phase 10 report](../../guideXOSServer/docs/APPMODEL_PHASE10_DEVELOPER_STUDIO_DOCUMENT_ACTIVATION.md).
