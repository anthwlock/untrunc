import Foundation
import Combine

class UntruncRunner: ObservableObject {
    @Published var log: String = ""
    @Published var isRunning: Bool = false
    @Published var lastOutputFile: URL? = nil

    private var process: Process?

    var binaryPath: String {
        // Look for bundled binary first, then fall back to PATH
        if let bundled = Bundle.main.path(forResource: "untrunc", ofType: nil) {
            return bundled
        }
        // Fall back to the built binary next to the app during development
        let devPath = FileManager.default.currentDirectoryPath
        let siblingPath = (devPath as NSString).appendingPathComponent("../untrunc-src/untrunc")
        if FileManager.default.fileExists(atPath: siblingPath) {
            return siblingPath
        }
        return "/usr/local/bin/untrunc"
    }

    func run(referenceFile: URL, corruptFile: URL, options: UntruncOptions) {
        guard !isRunning else { return }
        isRunning = true
        log = ""
        lastOutputFile = nil

        var args: [String] = []

        // Logging level
        switch options.logLevel {
        case .quiet:   args.append("-q")
        case .verbose: args.append("-v")
        case .veryVerbose: args.append("-vv")
        default: break
        }

        if options.stepUnknown       { args.append("-s") }
        if options.stretchVideo      { args.append("-sv") }
        if options.keepUnknown       { args.append("-k") }
        if options.skipOutput        { args.append("-dw") }
        if options.makeStreamable    { args.append("-ms") }
        if options.noInteractive     { args.append("-n") }

        args.append(referenceFile.path)
        args.append(corruptFile.path)

        appendLog("▶ Running untrunc \(args.joined(separator: " "))\n")

        // Predict output filename
        let ext = corruptFile.pathExtension
        let base = corruptFile.deletingPathExtension().lastPathComponent
        let outputName = "\(base)_fixed.\(ext)"
        let outputURL = corruptFile.deletingLastPathComponent().appendingPathComponent(outputName)

        DispatchQueue.global(qos: .userInitiated).async { [weak self] in
            guard let self else { return }

            let proc = Process()
            proc.executableURL = URL(fileURLWithPath: self.binaryPath)
            proc.arguments = args

            // Libraries are bundled inside the .app — no external deps needed.
            // DYLD_LIBRARY_PATH is intentionally not set; @rpath handles resolution.
            proc.environment = ProcessInfo.processInfo.environment

            let outPipe = Pipe()
            let errPipe = Pipe()
            proc.standardOutput = outPipe
            proc.standardError = errPipe

            // Stream stdout
            outPipe.fileHandleForReading.readabilityHandler = { [weak self] handle in
                let data = handle.availableData
                if let str = String(data: data, encoding: .utf8), !str.isEmpty {
                    self?.appendLog(str)
                }
            }
            // Stream stderr
            errPipe.fileHandleForReading.readabilityHandler = { [weak self] handle in
                let data = handle.availableData
                if let str = String(data: data, encoding: .utf8), !str.isEmpty {
                    self?.appendLog(str)
                }
            }

            self.process = proc

            do {
                try proc.run()
                proc.waitUntilExit()
            } catch {
                self.appendLog("❌ Failed to launch untrunc: \(error.localizedDescription)\n")
            }

            outPipe.fileHandleForReading.readabilityHandler = nil
            errPipe.fileHandleForReading.readabilityHandler = nil

            DispatchQueue.main.async { [weak self] in
                guard let self else { return }
                self.isRunning = false
                let status = proc.terminationStatus
                if status == 0 {
                    self.appendLog("\n✅ Done! Output: \(outputURL.path)\n")
                    if FileManager.default.fileExists(atPath: outputURL.path) {
                        self.lastOutputFile = outputURL
                    }
                } else {
                    self.appendLog("\n❌ Exited with status \(status)\n")
                }
            }
        }
    }

    func cancel() {
        process?.terminate()
        appendLog("\n⚠️ Cancelled.\n")
        isRunning = false
    }

    private func appendLog(_ text: String) {
        DispatchQueue.main.async {
            self.log += text
        }
    }
}

// MARK: - Options

enum LogLevel: String, CaseIterable, Identifiable {
    case normal = "Normal"
    case quiet = "Quiet (errors only)"
    case verbose = "Verbose"
    case veryVerbose = "Very Verbose"
    var id: String { rawValue }
}

struct UntruncOptions {
    var logLevel: LogLevel = .normal
    var stepUnknown: Bool = false
    var stretchVideo: Bool = false
    var keepUnknown: Bool = false
    var skipOutput: Bool = false
    var makeStreamable: Bool = false
    var noInteractive: Bool = true
}
