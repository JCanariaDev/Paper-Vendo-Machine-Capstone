from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_RIGHT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (SimpleDocTemplate, Paragraph, Spacer, Table,
                                TableStyle, PageBreak, KeepTogether)


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "output" / "pdf" / "Capstone_Refactor_Implementation_Guide.pdf"
BLUE = colors.HexColor("#2E74B5")
DARK_BLUE = colors.HexColor("#1F4D78")
INK = colors.HexColor("#0B2545")
MUTED = colors.HexColor("#667085")
HEADER_FILL = colors.HexColor("#E8EEF5")
CALLOUT_FILL = colors.HexColor("#F4F6F9")
WARNING_FILL = colors.HexColor("#FFF7E6")


def page_canvas(canvas, doc):
    canvas.saveState()
    canvas.setFont("Helvetica-Bold", 8)
    canvas.setFillColor(MUTED)
    canvas.drawString(doc.leftMargin, letter[1] - 0.56 * inch, "PAPER VENDO MACHINE | REFACTOR IMPLEMENTATION GUIDE")
    canvas.setFont("Helvetica", 8)
    canvas.drawRightString(letter[0] - doc.rightMargin, 0.48 * inch, f"Page {doc.page}")
    canvas.restoreState()


def p(text, style):
    return Paragraph(text, style)


def table(headers, rows, widths):
    data = [[p(h, styles["th"]) for h in headers]]
    for row in rows:
        data.append([p(str(cell), styles["td"]) for cell in row])
    out = Table(data, colWidths=widths, repeatRows=1, hAlign="LEFT")
    out.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), HEADER_FILL),
        ("TEXTCOLOR", (0, 0), (-1, 0), DARK_BLUE),
        ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#C8D0DA")),
        ("LEFTPADDING", (0, 0), (-1, -1), 6),
        ("RIGHTPADDING", (0, 0), (-1, -1), 6),
        ("TOPPADDING", (0, 0), (-1, -1), 5),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
    ]))
    return out


def callout(label, text, fill=CALLOUT_FILL):
    item = Table([[p(f"<b>{label}</b> {text}", styles["body"]) ]], colWidths=[6.5 * inch])
    item.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), fill),
        ("BOX", (0, 0), (-1, -1), 0.6, colors.HexColor("#B7C3D0")),
        ("LEFTPADDING", (0, 0), (-1, -1), 8),
        ("RIGHTPADDING", (0, 0), (-1, -1), 8),
        ("TOPPADDING", (0, 0), (-1, -1), 7),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
    ]))
    return item


base = getSampleStyleSheet()
styles = {
    "title": ParagraphStyle("title", parent=base["Title"], fontName="Helvetica-Bold", fontSize=22, leading=26, textColor=INK, spaceAfter=5),
    "subtitle": ParagraphStyle("subtitle", parent=base["Normal"], fontName="Helvetica", fontSize=11, leading=14, textColor=MUTED, spaceAfter=12),
    "kicker": ParagraphStyle("kicker", parent=base["Normal"], fontName="Helvetica-Bold", fontSize=9, leading=11, textColor=BLUE, spaceAfter=3),
    "h1": ParagraphStyle("h1", parent=base["Heading1"], fontName="Helvetica-Bold", fontSize=16, leading=19, textColor=BLUE, spaceBefore=14, spaceAfter=6),
    "body": ParagraphStyle("body", parent=base["Normal"], fontName="Helvetica", fontSize=9.5, leading=12, textColor=INK, spaceAfter=6),
    "bullet": ParagraphStyle("bullet", parent=base["Normal"], fontName="Helvetica", fontSize=9.5, leading=12, textColor=INK, leftIndent=18, firstLineIndent=-10, spaceAfter=4),
    "th": ParagraphStyle("th", parent=base["Normal"], fontName="Helvetica-Bold", fontSize=8, leading=9.5, textColor=DARK_BLUE, alignment=TA_CENTER),
    "td": ParagraphStyle("td", parent=base["Normal"], fontName="Helvetica", fontSize=8, leading=10, textColor=INK),
}


def bullets(items):
    return [p("&#8226; " + item, styles["bullet"]) for item in items]


def main():
    OUT.parent.mkdir(parents=True, exist_ok=True)
    doc = SimpleDocTemplate(str(OUT), pagesize=letter, leftMargin=inch, rightMargin=inch,
                            topMargin=0.82 * inch, bottomMargin=0.72 * inch,
                            title="Paper Vendo Machine Capstone Refactor")
    story = []
    story += [p("IMPLEMENTATION GUIDE", styles["kicker"]), p("Paper Vendo Machine Capstone Refactor", styles["title"]),
              p("Machine flow, database contract, web reporting, wiring additions, and deployment checks", styles["subtitle"])]
    story += [table(["Prepared", "Scope", "Status"], [["2 August 2026", "Mega, ESP32, Supabase, PERN website", "Implementation refactor; hardware validation required"]], [1.25*inch, 3.05*inch, 2.2*inch]), Spacer(1, 10)]
    story += [callout("Release rule.", "The machine verifies and records the full change payment before it receives a dispense plan. It never relies on a manual D button or a website calculation."), Spacer(1, 6)]
    story += [p("What changed", styles["h1"])]
    story += bullets([
        "A complete cart is reserved atomically before any motor moves, including products, user units, physical output quantity, stock, and exact change.",
        "The Mega returns verified change first through a hopper sensor. Only CHANGE_OK allows the ESP32 to request the dispense plan.",
        "Transaction lines store units_requested separately from qty_requested and qty_dispensed. For paper, units are the customer choice and quantity is physical sheets.",
        "The website reads stored units and price snapshots; it no longer reconstructs units from amount, sheet settings, or quantities after the sale.",
        "Four paper channels are physical feeders by size. Budget and Standard are logical catalog products that may share a feeder only when their stock is physically identical.",
    ])
    story += [p("Final transaction flow", styles["h1"])]
    for i, item in enumerate([
        "Customer inserts coins. The Mega maintains credit in pesos and sends exact cents only when checkout begins.",
        "Customer selects Paper, then Budget or Standard, then one of four sizes; or selects one of three pen channels. The cart contains requested units, not calculated sheets.",
        "Mega inhibits the coin acceptor and sends one RESERVE message for the whole cart. ESP32 calls machine_reserve_transaction, which validates price, units, stock, shared feeder capacity, and exact change in one database transaction.",
        "ESP32 returns RESERVED with transaction ID, subtotal, and change due. Mega drives the change hopper and counts each exit-sensor edge.",
        "Only after every change coin is sensor-confirmed does Mega send CHANGE_OK. ESP32 marks CHANGE_PAID and returns the physical PLAN.",
        "Mega dispenses each plan item. Paper needs an exit-sensor cycle for each physical sheet; pens need the IR drop confirmation. Mega sends actual quantities in FINISH.",
        "ESP32 calls machine_finish_transaction. The database commits inventory consumption, marks lines DISPENSED or FAILED, and exposes stored results to the website.",
    ]):
        story.append(p(f"<b>{i+1}.</b> {item}", styles["bullet"]))
    story += [callout("Failure behavior.", "If exact change cannot be confirmed, Mega sends CHANGE_FAIL and the reservation is cancelled. If change was confirmed but a dispenser sensor fails, the transaction is stored as FAILED_DISPENSE with actual output; this requires operator resolution.", WARNING_FILL), Spacer(1, 4)]
    story += [p("Data model and calculation rule", styles["h1"]), table(["Concept", "Stored field", "Meaning"], [
        ["Customer choice", "units_requested", "How many sellable units the user selected."],
        ["Paper physical output", "qty_requested", "units_requested x sheets_per_unit_snapshot."],
        ["Confirmed output", "qty_dispensed", "Sensor-confirmed sheets or pens actually released."],
        ["Price evidence", "unit_price_cents", "Price at reservation; later setting edits cannot rewrite history."],
        ["Money", "*_cents", "Integer cent values avoid floating-point money errors."],
    ], [1.2*inch, 1.75*inch, 3.55*inch]), Spacer(1, 5),
    p("<b>Formula:</b> subtotal_cents = sum(unit_price_cents x units_requested). change_due_cents = credit_received_cents - subtotal_cents. For paper, qty_requested = units_requested x sheets_per_unit_snapshot.", styles["body"])]

    story += [p("Wiring additions only", styles["h1"]), p("Existing pen steppers, pen IR sensors, TFT, coin acceptor, OLED, servos, and Mega-ESP UART remain connected as they are. Add only the following paths:", styles["body"]),
              table(["Hardware", "Mega pins", "Purpose"], [
                ["Paper drivers 1 to 4", "STEP/DIR: 32/33, 34/35, 36/37, 38/39; shared EN: 40", "Drive one adjustable paper feeder per physical size."],
                ["Paper exit sensors 1 to 4", "41, 42, 43, 44", "Confirm every sheet after it leaves its channel."],
                ["Change hopper motor driver", "14", "Releases reserved P1 change coins."],
                ["Change hopper coin sensor", "15", "Confirms every released change coin before dispensing."],
                ["Coin acceptor inhibit", "16", "Freezes coin acceptance after checkout starts so the credit snapshot cannot change mid-transaction."],
                ["Mega to ESP UART", "Mega Serial1 18/19 to ESP Serial2 16/17 through level shifting", "Carries reservation and result protocol; protect ESP 3.3 V RX."],
              ], [1.5*inch, 2.35*inch, 2.65*inch]), Spacer(1, 8),
              callout("Mechanical constraint.", "Four paper compartments support eight catalog choices only when Budget and Standard for the same size use the same physical paper. If brands differ in stock, color, weight, or finish, use eight physical feeders.", WARNING_FILL),
              p("File-by-file implementation summary", styles["h1"]),
              p("Key line ranges identify the main implementation areas, not every supporting line.", styles["body"]),
              table(["File", "Key lines", "Specific change and purpose"], [
                ["Cloud_Paper_Vendo.sql", "1-328; 152-326", "Clean development schema: channels, exact change, transaction header/lines, atomic reserve/change/finish RPCs. Purpose: authoritative, auditable data flow."],
                ["updated_arduino_mega.ino", "20-36; 178-226; 571-693; 949-1258", "Pins, coin inhibit, brand selection, display price, cart reservation, verified change, sensor routines, FINISH results. Purpose: stable credit snapshot, change before dispense, and machine confirmation."],
                ["production_esp32.ino", "1-229; 125-212", "Serial protocol and Supabase RPC gateway. Purpose: machine-to-database state changes."],
                ["backend/routes/machine.js", "1-276; 102-257", "Stored units/output, inventory updates, completed-line analytics. Purpose: stop frontend inference."],
                ["Transactions.jsx", "1-72; 59-60", "Stored units, expected/actual output, snapshots, statuses. Purpose: auditability."],
                ["Reports.jsx", "56-145; 319-340", "Uses units_requested and product snapshots. Purpose: accurate historical reports."],
                ["backend/routes/auth.js", "30-42", "Removes plaintext password fallback. Purpose: bcrypt-only login."],
              ], [1.4*inch, 1.35*inch, 3.75*inch])]

    story += [p("Deployment and calibration checklist", styles["h1"])]
    story += bullets([
        "Back up real Supabase data. Cloud_Paper_Vendo.sql is a clean-reset development script and drops the listed tables.",
        "Run SQL on the development database, then enter real stock, prices, sheets per unit, and hopper coin count.",
        "Configure real Wi-Fi and Supabase values in production_esp32.ino. Do not place credentials in source control.",
        "Install and calibrate four paper modules: driver direction, step timing, separator, and exit sensor. A paper sheet counts only on clear -> blocked -> clear.",
        "Install a motor driver between Mega D14 and the hopper motor; never drive the motor directly from a Mega pin. Adjust active level for your module.",
        "Run full-path tests: pen, multi-sheet paper, mixed cart, zero change, valid change, no exact change, sensor failure, and Wi-Fi loss before and after change payment.",
    ])
    story += [p("Verification completed in this refactor", styles["h1"])]
    story += bullets([
        "Backend JavaScript syntax checked with Node.",
        "Frontend production build completed successfully with Vite.",
        "Mega sketch structure checked for balanced braces. Physical Arduino compile and hardware tests remain required because this workspace has no Arduino CLI or hardware access.",
        "This PDF was rendered and visually inspected before delivery.",
    ])
    story += [p("Operator acceptance criteria", styles["h1"])]
    story += bullets([
        "No good is released unless database reservation succeeds and the change sensor verifies the change due.",
        "A website transaction shows selected units, actual physical output, price snapshot, status, and timestamp without recomputing them.",
        "Paper inventory reduces by confirmed physical sheets; pen inventory reduces by confirmed pens.",
        "A sensor-short dispense is visible as a failed line and failed transaction, rather than silently marked successful.",
    ])
    doc.build(story, onFirstPage=page_canvas, onLaterPages=page_canvas)
    print(OUT)


if __name__ == "__main__":
    main()
