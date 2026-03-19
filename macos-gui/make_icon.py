#!/usr/bin/env python3
"""
Generates Untrunc.icns app icon using only built-in macOS tools.
Draws a film-reel + wrench concept with a dark gradient background.
"""
import subprocess, os, shutil, math, struct, zlib

SIZES = [16, 32, 64, 128, 256, 512, 1024]

def draw_icon(size):
    """Generate icon PNG at given size using a Swift one-liner via NSGraphicsContext."""
    script = f"""
import AppKit
import CoreGraphics

let sz = CGFloat({size})
let rep = NSBitmapImageRep(
    bitmapDataPlanes: nil, pixelsWide: {size}, pixelsHigh: {size},
    bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true,
    isPlanar: false, colorSpaceName: .deviceRGB,
    bytesPerRow: 0, bitsPerPixel: 0)!

NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
let ctx = NSGraphicsContext.current!.cgContext

// ---- Background: dark rounded rect with gradient ----
let pad = sz * 0.0
let rect = CGRect(x: pad, y: pad, width: sz - pad*2, height: sz - pad*2)
let radius = sz * 0.22

let path = CGMutablePath()
path.addRoundedRect(in: rect, cornerWidth: radius, cornerHeight: radius)
ctx.addPath(path)
ctx.clip()

let colors = [CGColor(red:0.08, green:0.09, blue:0.14, alpha:1),
              CGColor(red:0.13, green:0.16, blue:0.26, alpha:1)]
let locs: [CGFloat] = [0, 1]
let grad = CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(),
                      colors: colors as CFArray, locations: locs)!
ctx.drawLinearGradient(grad,
    start: CGPoint(x: sz*0.3, y: sz),
    end:   CGPoint(x: sz*0.7, y: 0),
    options: [])

// ---- Film strip body ----
let blue = CGColor(red:0.24, green:0.52, blue:1.0, alpha:1)
let filmH = sz * 0.36
let filmY = sz * 0.32
let filmX = sz * 0.14
let filmW = sz * 0.72
let filmR = sz * 0.04

let film = CGMutablePath()
film.addRoundedRect(in: CGRect(x:filmX, y:filmY, width:filmW, height:filmH),
                    cornerWidth: filmR, cornerHeight: filmR)
ctx.addPath(film)
ctx.setFillColor(blue)
ctx.fillPath()

// ---- Sprocket holes ----
let holeR = sz * 0.038
let holeY1 = filmY + filmH * 0.18
let holeY2 = filmY + filmH * 0.64
let holeCount = 5
let holeStep = filmW / CGFloat(holeCount + 1)
ctx.setFillColor(CGColor(red:0.08, green:0.09, blue:0.14, alpha:0.9))
for i in 1...holeCount {{
    let hx = filmX + holeStep * CGFloat(i)
    ctx.fillEllipse(in: CGRect(x: hx - holeR, y: holeY1 - holeR, width: holeR*2, height: holeR*2))
    ctx.fillEllipse(in: CGRect(x: hx - holeR, y: holeY2 - holeR, width: holeR*2, height: holeR*2))
}}

// ---- Wrench (repair symbol) overlapping bottom-right ----
let wCx = sz * 0.64
let wCy = sz * 0.38
let wR  = sz * 0.22

// wrench handle
ctx.setStrokeColor(CGColor(red:1, green:0.78, blue:0.2, alpha:1))
ctx.setLineWidth(sz * 0.075)
ctx.setLineCap(.round)
ctx.move(to: CGPoint(x: wCx - wR*0.5, y: wCy - wR*0.5))
ctx.addLine(to: CGPoint(x: wCx + wR*0.55, y: wCy + wR*0.55))
ctx.strokePath()

// wrench head circle
ctx.setFillColor(CGColor(red:1, green:0.78, blue:0.2, alpha:1))
let headR = wR * 0.38
ctx.fillEllipse(in: CGRect(x: wCx - wR*0.5 - headR, y: wCy - wR*0.5 - headR,
                            width: headR*2, height: headR*2))
// cut out center
ctx.setFillColor(CGColor(red:0.12, green:0.14, blue:0.20, alpha:1))
let cutR = headR * 0.48
ctx.fillEllipse(in: CGRect(x: wCx - wR*0.5 - cutR, y: wCy - wR*0.5 - cutR,
                            width: cutR*2, height: cutR*2))

// ---- Lightning bolt (corrupt indicator) top-left ----
ctx.setFillColor(CGColor(red:1, green:0.35, blue:0.25, alpha:0.92))
let bx = sz * 0.19
let by = sz * 0.60
let bw = sz * 0.11
let bh = sz * 0.20
let bolt = CGMutablePath()
bolt.move(to:    CGPoint(x: bx + bw*0.55, y: by + bh))
bolt.addLine(to: CGPoint(x: bx + bw,      y: by + bh*0.52))
bolt.addLine(to: CGPoint(x: bx + bw*0.62, y: by + bh*0.52))
bolt.addLine(to: CGPoint(x: bx + bw*0.45, y: by))
bolt.addLine(to: CGPoint(x: bx,           y: by + bh*0.48))
bolt.addLine(to: CGPoint(x: bx + bw*0.38, y: by + bh*0.48))
bolt.closeSubpath()
ctx.addPath(bolt)
ctx.fillPath()

// ---- Output PNG ----
let data = rep.representation(using: .png, properties: [:])!
try! data.write(to: URL(fileURLWithPath: "/tmp/icon_{size}.png"))
"""
    result = subprocess.run(["swift", "-"], input=script, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  Swift error for {size}:", result.stderr[:300])
        return False
    return True


def build_iconset(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    mappings = [
        (16,  "icon_16x16.png"),
        (32,  "icon_16x16@2x.png"),
        (32,  "icon_32x32.png"),
        (64,  "icon_32x32@2x.png"),
        (128, "icon_128x128.png"),
        (256, "icon_128x128@2x.png"),
        (256, "icon_256x256.png"),
        (512, "icon_256x256@2x.png"),
        (512, "icon_512x512.png"),
        (1024,"icon_512x512@2x.png"),
    ]
    needed = sorted(set(s for s, _ in mappings))
    for sz in needed:
        print(f"  Rendering {sz}x{sz}…")
        if not draw_icon(sz):
            return False
    for sz, name in mappings:
        src = f"/tmp/icon_{sz}.png"
        dst = os.path.join(out_dir, name)
        shutil.copy(src, dst)
    return True


if __name__ == "__main__":
    iconset = "/tmp/Untrunc.iconset"
    icns    = "/Users/macbook/Desktop/AI Vibe Code/untrunc/UntruncGUI/Sources/UntruncGUI/AppIcon.icns"

    print("Drawing icon frames…")
    if not build_iconset(iconset):
        print("FAILED")
        raise SystemExit(1)

    print("Compiling .icns…")
    r = subprocess.run(["iconutil", "-c", "icns", iconset, "-o", icns], capture_output=True, text=True)
    if r.returncode != 0:
        print("iconutil error:", r.stderr)
        raise SystemExit(1)
    print(f"✅ Icon written to {icns}")
