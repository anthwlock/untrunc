import SwiftUI
import AppKit

struct ContentView: View {
    @StateObject private var runner = UntruncRunner()
    @State private var referenceFile: URL? = nil
    @State private var corruptFile: URL? = nil
    @State private var options = UntruncOptions()
    @State private var logScrollProxy: ScrollViewProxy? = nil

    private let videoTypes = ["mp4", "m4v", "mov", "3gp", "MP4", "M4V", "MOV", "3GP"]

    var canRun: Bool {
        referenceFile != nil && corruptFile != nil && !runner.isRunning
    }

    var body: some View {
        HSplitView {
            // Left panel: controls
            VStack(alignment: .leading, spacing: 20) {
                headerSection
                fileDropSection
                optionsSection
                actionSection
                Spacer()
            }
            .padding(24)
            .frame(minWidth: 340, maxWidth: 420)

            // Right panel: log output
            logPanel
        }
        .frame(minWidth: 800, minHeight: 520)
        .background(Color(NSColor.windowBackgroundColor))
    }

    // MARK: - Header

    private var headerSection: some View {
        VStack(alignment: .leading, spacing: 4) {
            HStack(spacing: 10) {
                Image(systemName: "film.stack")
                    .font(.system(size: 28))
                    .foregroundColor(.accentColor)
                VStack(alignment: .leading, spacing: 2) {
                    Text("Untrunc")
                        .font(.title2)
                        .fontWeight(.bold)
                    Text("Restore truncated MP4/MOV video files")
                        .font(.caption)
                        .foregroundColor(.secondary)
                }
            }
            Divider()
        }
    }

    // MARK: - File Drop Areas

    private var fileDropSection: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Files")
                .font(.headline)

            FileDropZone(
                label: "Reference File",
                systemImage: "checkmark.seal.fill",
                tint: .green,
                hint: "A working video from the same source/camera",
                file: $referenceFile,
                allowedTypes: videoTypes
            )

            FileDropZone(
                label: "Corrupt File",
                systemImage: "exclamationmark.triangle.fill",
                tint: .orange,
                hint: "The truncated or damaged video to repair",
                file: $corruptFile,
                allowedTypes: videoTypes
            )
        }
    }

    // MARK: - Options

    private var optionsSection: some View {
        VStack(alignment: .leading, spacing: 10) {
            Text("Options")
                .font(.headline)

            Picker("Log level", selection: $options.logLevel) {
                ForEach(LogLevel.allCases) { level in
                    Text(level.rawValue).tag(level)
                }
            }
            .pickerStyle(.menu)

            Group {
                Toggle("Step through unknown sequences (-s)", isOn: $options.stepUnknown)
                Toggle("Stretch video to match audio (-sv)", isOn: $options.stretchVideo)
                Toggle("Keep unknown sequences (-k)", isOn: $options.keepUnknown)
                Toggle("Skip output file / analysis only (-dw)", isOn: $options.skipOutput)
                Toggle("Make streamable output (-ms)", isOn: $options.makeStreamable)
            }
            .toggleStyle(.checkbox)
            .font(.callout)
        }
    }

    // MARK: - Action Buttons

    private var actionSection: some View {
        VStack(spacing: 10) {
            if runner.isRunning {
                HStack {
                    ProgressView()
                        .scaleEffect(0.8)
                    Text("Repairing…")
                        .font(.callout)
                        .foregroundColor(.secondary)
                    Spacer()
                    Button("Cancel") { runner.cancel() }
                        .foregroundColor(.red)
                }
            } else {
                Button(action: repair) {
                    Label("Repair Video", systemImage: "wrench.and.screwdriver.fill")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
                .disabled(!canRun)
            }

            if let outputFile = runner.lastOutputFile {
                Button(action: { revealInFinder(outputFile) }) {
                    Label("Show Output in Finder", systemImage: "folder")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.bordered)
            }
        }
    }

    // MARK: - Log Panel

    private var logPanel: some View {
        VStack(alignment: .leading, spacing: 0) {
            HStack {
                Text("Output")
                    .font(.headline)
                    .padding(.horizontal, 16)
                    .padding(.vertical, 10)
                Spacer()
                if !runner.log.isEmpty {
                    Button("Clear") { runner.log = "" }
                        .buttonStyle(.plain)
                        .foregroundColor(.secondary)
                        .font(.callout)
                        .padding(.trailing, 16)
                }
            }
            .background(Color(NSColor.windowBackgroundColor))

            Divider()

            ScrollViewReader { proxy in
                ScrollView {
                    Text(runner.log.isEmpty ? "Output will appear here…" : runner.log)
                        .font(.system(.caption, design: .monospaced))
                        .foregroundColor(runner.log.isEmpty ? .secondary : .primary)
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(12)
                        .id("logBottom")
                }
                .background(Color(NSColor.textBackgroundColor))
                .onChange(of: runner.log) { _ in
                    withAnimation {
                        proxy.scrollTo("logBottom", anchor: .bottom)
                    }
                }
            }
        }
        .frame(minWidth: 380)
    }

    // MARK: - Actions

    private func repair() {
        guard let ref = referenceFile, let corrupt = corruptFile else { return }
        runner.run(referenceFile: ref, corruptFile: corrupt, options: options)
    }

    private func revealInFinder(_ url: URL) {
        NSWorkspace.shared.activateFileViewerSelecting([url])
    }
}

// MARK: - File Drop Zone

struct FileDropZone: View {
    let label: String
    let systemImage: String
    let tint: Color
    let hint: String
    @Binding var file: URL?
    let allowedTypes: [String]

    @State private var isTargeted = false

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Label(label, systemImage: systemImage)
                .font(.subheadline)
                .fontWeight(.medium)
                .foregroundColor(tint)

            ZStack {
                RoundedRectangle(cornerRadius: 10)
                    .fill(isTargeted
                          ? tint.opacity(0.12)
                          : Color(NSColor.controlBackgroundColor))
                    .overlay(
                        RoundedRectangle(cornerRadius: 10)
                            .strokeBorder(
                                isTargeted ? tint : Color(NSColor.separatorColor),
                                style: StrokeStyle(lineWidth: isTargeted ? 2 : 1, dash: [6])
                            )
                    )

                if let file {
                    HStack(spacing: 10) {
                        Image(systemName: "doc.fill")
                            .foregroundColor(tint)
                        VStack(alignment: .leading, spacing: 2) {
                            Text(file.lastPathComponent)
                                .font(.callout)
                                .fontWeight(.medium)
                                .lineLimit(1)
                                .truncationMode(.middle)
                            Text(file.deletingLastPathComponent().path)
                                .font(.caption2)
                                .foregroundColor(.secondary)
                                .lineLimit(1)
                                .truncationMode(.head)
                        }
                        Spacer()
                        Button {
                            self.file = nil
                        } label: {
                            Image(systemName: "xmark.circle.fill")
                                .foregroundColor(.secondary)
                        }
                        .buttonStyle(.plain)
                    }
                    .padding(.horizontal, 12)
                } else {
                    VStack(spacing: 6) {
                        Image(systemName: "arrow.down.circle")
                            .font(.title2)
                            .foregroundColor(isTargeted ? tint : .secondary)
                        Text("Drop file here or")
                            .font(.callout)
                            .foregroundColor(.secondary)
                        Button("Choose File…") { chooseFile() }
                            .buttonStyle(.plain)
                            .foregroundColor(.accentColor)
                            .font(.callout)
                        Text(hint)
                            .font(.caption2)
                            .foregroundColor(.secondary)
                            .multilineTextAlignment(.center)
                    }
                    .padding(.vertical, 8)
                }
            }
            .frame(height: 90)
            .contentShape(Rectangle())
            .onTapGesture { if file == nil { chooseFile() } }
            .onDrop(of: ["public.file-url"], isTargeted: $isTargeted) { providers in
                handleDrop(providers)
            }
        }
    }

    private func chooseFile() {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = false
        panel.canChooseDirectories = false
        panel.allowedContentTypes = []
        panel.title = "Select \(label)"
        panel.message = hint
        if panel.runModal() == .OK, let url = panel.url {
            self.file = url
        }
    }

    private func handleDrop(_ providers: [NSItemProvider]) -> Bool {
        guard let provider = providers.first else { return false }
        provider.loadItem(forTypeIdentifier: "public.file-url", options: nil) { item, _ in
            DispatchQueue.main.async {
                if let data = item as? Data,
                   let url = URL(dataRepresentation: data, relativeTo: nil) {
                    let ext = url.pathExtension.lowercased()
                    if self.allowedTypes.map({ $0.lowercased() }).contains(ext) {
                        self.file = url
                    }
                }
            }
        }
        return true
    }
}

#Preview {
    ContentView()
}
