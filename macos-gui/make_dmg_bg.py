#!/usr/bin/env python3
"""Generate a 660x400 DMG background image using Swift/AppKit."""
import subprocess, sys

script = r"""
import AppKit
import CoreGraphics

let W = CGFloat(660), H = CGFloat(400)
let rep = NSBitmapImageRep(
    bitmapDataPlanes: nil, pixelsWide: 660, pixelsHigh: 400,
    bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true,
    isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
let ctx = NSGraphicsContext.current!.cgContext

// Background gradient
let bg1 = CGColor(red:0.06, green:0.07, blue:0.11, alpha:1)
let bg2 = CGColor(red:0.10, green:0.13, blue:0.22, alpha:1)
let grad = CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(),
    colors: [bg1, bg2] as CFArray, locations: [0, 1] as [CGFloat])!
ctx.drawLinearGradient(grad, start: CGPoint(x:0,y:H), end: CGPoint(x:W,y:0), options:[])

// Subtle film strip texture lines across top and bottom
ctx.setStrokeColor(CGColor(red:1,green:1,blue:1,alpha:0.04))
ctx.setLineWidth(1)
for i in stride(from: 0, to: Int(W), by: 18) {
    ctx.move(to: CGPoint(x: CGFloat(i), y: 0))
    ctx.addLine(to: CGPoint(x: CGFloat(i), y: H))
}
ctx.strokePath()

// Center divider glow
let divX = W * 0.5
ctx.setStrokeColor(CGColor(red:0.24, green:0.52, blue:1.0, alpha:0.18))
ctx.setLineWidth(1)
ctx.move(to: CGPoint(x: divX, y: 60))
ctx.addLine(to: CGPoint(x: divX, y: H - 60))
ctx.strokePath()

// Arrow from left (app) to right (Applications)
let arrowY = H * 0.52
let ax1 = W * 0.38, ax2 = W * 0.62
ctx.setStrokeColor(CGColor(red:0.24, green:0.52, blue:1.0, alpha:0.55))
ctx.setLineWidth(2.5)
ctx.setLineCap(.round)
ctx.move(to: CGPoint(x: ax1, y: arrowY))
ctx.addLine(to: CGPoint(x: ax2 - 14, y: arrowY))
ctx.strokePath()
// arrowhead
ctx.setFillColor(CGColor(red:0.24, green:0.52, blue:1.0, alpha:0.55))
let arrow = CGMutablePath()
arrow.move(to:    CGPoint(x: ax2,      y: arrowY))
arrow.addLine(to: CGPoint(x: ax2 - 14, y: arrowY - 7))
arrow.addLine(to: CGPoint(x: ax2 - 14, y: arrowY + 7))
arrow.closeSubpath()
ctx.addPath(arrow)
ctx.fillPath()

// Labels
let titleAttrs: [NSAttributedString.Key: Any] = [
    .font: NSFont.systemFont(ofSize: 22, weight: .bold),
    .foregroundColor: NSColor(calibratedRed:0.92, green:0.94, blue:1.0, alpha:1)
]
let subAttrs: [NSAttributedString.Key: Any] = [
    .font: NSFont.systemFont(ofSize: 12, weight: .regular),
    .foregroundColor: NSColor(calibratedRed:0.55, green:0.62, blue:0.80, alpha:1)
]
let instrAttrs: [NSAttributedString.Key: Any] = [
    .font: NSFont.systemFont(ofSize: 13, weight: .medium),
    .foregroundColor: NSColor(calibratedRed:0.70, green:0.78, blue:1.0, alpha:0.85)
]

func drawCentered(_ s: String, attrs: [NSAttributedString.Key:Any], cx: CGFloat, cy: CGFloat) {
    let ns = NSAttributedString(string: s, attributes: attrs)
    let sz = ns.size()
    ns.draw(at: CGPoint(x: cx - sz.width/2, y: cy - sz.height/2))
}

drawCentered("Untrunc", attrs: titleAttrs, cx: W*0.5, cy: H*0.82)
drawCentered("Drag Untrunc to your Applications folder", attrs: instrAttrs, cx: W*0.5, cy: H*0.70)
drawCentered("Restore truncated MP4, MOV & 3GP video files", attrs: subAttrs, cx: W*0.5, cy: H*0.60)

let data = rep.representation(using: .png, properties: [:])!
try! data.write(to: URL(fileURLWithPath: "/tmp/dmg_background.png"))
print("done")
"""
r = subprocess.run(["swift", "-"], input=script, capture_output=True, text=True)
if r.returncode != 0:
    print("Error:", r.stderr[:400])
    sys.exit(1)
print("✅ DMG background written to /tmp/dmg_background.png")
