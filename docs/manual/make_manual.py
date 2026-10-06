#!/usr/bin/env python3
"""Generates the user manuals as PDF:
    docs/MIDI-Composer-Manual.pdf     English (bundled with the app, Options > User manual)
    docs/MIDI-Composer-Manual-da.pdf  Danish (attached to GitHub Releases)
English content lives in manual_en.py, Danish content in content_da() below.

    python3 -m venv .venv && .venv/bin/pip install reportlab
    .venv/bin/python docs/manual/make_manual.py

Fonts: Arial (+ Arial Unicode for symbols) from macOS; falls back to Helvetica.
"""
import os
import re
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (Image, KeepTogether, PageBreak, Paragraph, SimpleDocTemplate, Spacer, Table,
                                TableStyle)
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT_EN = os.path.join(ROOT, "docs", "MIDI-Composer-Manual.pdf")      # English: bundled with the app
OUT_DA = os.path.join(ROOT, "docs", "MIDI-Composer-Manual-da.pdf")   # Danish: GitHub Releases download
ICON = os.path.join(ROOT, "resources", "icon.png")

# ------------------------------------------------------------------ fonts
SUP = "/System/Library/Fonts/Supplemental/"
try:
    pdfmetrics.registerFont(TTFont("Body", SUP + "Arial.ttf"))
    pdfmetrics.registerFont(TTFont("Body-Bold", SUP + "Arial Bold.ttf"))
    pdfmetrics.registerFont(TTFont("Sym", SUP + "Arial Unicode.ttf"))
    from reportlab.pdfbase.pdfmetrics import registerFontFamily
    registerFontFamily("Body", normal="Body", bold="Body-Bold", italic="Body", boldItalic="Body-Bold")
    BODY, BOLD, SYM = "Body", "Body-Bold", "Sym"
except Exception:
    try:  # Linux: Liberation Sans (Arial metrics) + DejaVu Sans for symbols
        pdfmetrics.registerFont(TTFont("Body", "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"))
        pdfmetrics.registerFont(TTFont("Body-Bold", "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"))
        pdfmetrics.registerFont(TTFont("Sym", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"))
        from reportlab.pdfbase.pdfmetrics import registerFontFamily
        registerFontFamily("Body", normal="Body", bold="Body-Bold", italic="Body", boldItalic="Body-Bold")
        BODY, BOLD, SYM = "Body", "Body-Bold", "Sym"
    except Exception:
        BODY, BOLD, SYM = "Helvetica", "Helvetica-Bold", "Helvetica"

ACCENT = colors.HexColor("#2a7f9e")
WARM = colors.HexColor("#6b4a12")
LIGHT = colors.HexColor("#eef3f6")
GRID = colors.HexColor("#c9d3db")


def sym(text):
    """Wrap symbols Arial lacks in the Unicode font."""
    return re.sub(r"([♯♭→←↑↓↔⌘⌥⇧])", lambda m: f'<font name="{SYM}">{m.group(1)}</font>', text)


st = {
    "title": ParagraphStyle("title", fontName=BOLD, fontSize=30, leading=36, alignment=TA_CENTER, textColor=ACCENT),
    "subtitle": ParagraphStyle("subtitle", fontName=BODY, fontSize=14, leading=20, alignment=TA_CENTER, textColor=colors.HexColor("#555555")),
    "h1": ParagraphStyle("h1", fontName=BOLD, fontSize=19, leading=24, spaceBefore=4, spaceAfter=10, textColor=ACCENT),
    "h2": ParagraphStyle("h2", fontName=BOLD, fontSize=13.5, leading=18, spaceBefore=12, spaceAfter=5, textColor=colors.HexColor("#1f4f63")),
    "body": ParagraphStyle("body", fontName=BODY, fontSize=10.5, leading=15, spaceAfter=6),
    "bullet": ParagraphStyle("bullet", fontName=BODY, fontSize=10.5, leading=15, leftIndent=14, bulletIndent=3, spaceAfter=2),
    "num": ParagraphStyle("num", fontName=BODY, fontSize=10.5, leading=15, leftIndent=16, bulletIndent=0, spaceAfter=2),
    "cell": ParagraphStyle("cell", fontName=BODY, fontSize=9.5, leading=12.5),
    "cellb": ParagraphStyle("cellb", fontName=BOLD, fontSize=9.5, leading=12.5, textColor=colors.white),
    "tip": ParagraphStyle("tip", fontName=BODY, fontSize=10, leading=14, textColor=WARM),
    "code": ParagraphStyle("code", fontName="Courier", fontSize=9, leading=12, leftIndent=8, textColor=colors.HexColor("#222222")),
    "toc1": ParagraphStyle("toc1", fontName=BODY, fontSize=11, leading=17, leftIndent=6),
}

story = []
TOC_TITLE = ["Contents"]


def h1(t):
    p = Paragraph(sym(t), st["h1"])
    p._toc = t
    story.append(p)


def h2(t):
    story.append(Paragraph(sym(t), st["h2"]))


def p(t):
    story.append(Paragraph(sym(t), st["body"]))


def bullets(items):
    for i in items:
        story.append(Paragraph(sym(i), st["bullet"], bulletText="•"))
    story.append(Spacer(1, 4))


def steps(items):
    for n, i in enumerate(items, 1):
        story.append(Paragraph(sym(i), st["num"], bulletText=f"{n}."))
    story.append(Spacer(1, 4))


def code(t):
    story.append(Paragraph(t.replace(" ", "&nbsp;").replace("\n", "<br/>"), st["code"]))
    story.append(Spacer(1, 6))


def tip(t):
    tbl = Table([[Paragraph(sym("<b>Tip:</b> " + t), st["tip"])]], colWidths=[165 * mm])
    tbl.setStyle(TableStyle([("BACKGROUND", (0, 0), (-1, -1), colors.HexColor("#fbf3e4")),
                             ("BOX", (0, 0), (-1, -1), 0.6, colors.HexColor("#e2c48d")),
                             ("LEFTPADDING", (0, 0), (-1, -1), 8), ("RIGHTPADDING", (0, 0), (-1, -1), 8),
                             ("TOPPADDING", (0, 0), (-1, -1), 6), ("BOTTOMPADDING", (0, 0), (-1, -1), 6)]))
    story.append(tbl)
    story.append(Spacer(1, 8))


def table(header, rows, widths):
    data = [[Paragraph(sym(h), st["cellb"]) for h in header]]
    data += [[Paragraph(sym(c), st["cell"]) for c in r] for r in rows]
    tbl = Table(data, colWidths=[w * mm for w in widths], repeatRows=1)
    tbl.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, 0), ACCENT),
        ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, LIGHT]),
        ("GRID", (0, 0), (-1, -1), 0.4, GRID),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 5), ("RIGHTPADDING", (0, 0), (-1, -1), 5),
        ("TOPPADDING", (0, 0), (-1, -1), 3), ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
    ]))
    story.append(tbl)
    story.append(Spacer(1, 10))


# ================================================================== content
def content_da():
    """Danish manual (download from GitHub Releases)."""
    TOC_TITLE[0] = "Indhold"
    # ---- cover
    story.append(Spacer(1, 45 * mm))
    if os.path.exists(ICON):
        story.append(Image(ICON, width=38 * mm, height=38 * mm))
    story.append(Spacer(1, 10 * mm))
    story.append(Paragraph("MIDI Composer", st["title"]))
    story.append(Spacer(1, 4 * mm))
    story.append(Paragraph("Brugermanual", st["subtitle"]))
    story.append(Spacer(1, 3 * mm))
    story.append(Paragraph("Easy Midi Composer – komponér med akkorder, skalaer og indbyggede instrumenter", st["subtitle"]))
    story.append(PageBreak())

    # ---- toc
    story.append(Paragraph(TOC_TITLE[0], st["h1"]))
    toc = TableOfContents()
    toc.levelStyles = [st["toc1"]]
    toc.dotsMinLevel = 0
    story.append(toc)
    story.append(PageBreak())

    # ---- 1
    h1("1. Velkommen")
    p("MIDI Composer er et let program til at skrive musik med MIDI – idéer, akkordforløb og hele arrangementer – uden "
      "at skulle sætte et stort og kompliceret DAW-projekt op. Du kan spille ind fra et MIDI-keyboard, tegne toner med "
      "musen, lade programmet bygge akkorder ud fra en skala og høre det hele med indbyggede instrumenter eller dine egne "
      "plugins.")
    p("Når du er færdig, kan du eksportere til en MIDI-fil, som alle musikprogrammer kan åbne, eller til MusicXML, så du "
      "kan få noderne vist og skrevet ud i fx MuseScore.")
    h2("Det kan programmet")
    bullets([
        "Flere spor, hvert med sit eget instrument, farve, lydstyrke, panorering og MIDI-kanal.",
        "Piano roll til at tegne, flytte, forlænge, kopiere og slette toner.",
        "<b>Akkordtilstand</b>: én tangent giver en hel akkord i den valgte skala – med omvendinger, 7'ere og 9'ere.",
        "Forslag til næste akkord ud fra kvintcirklen.",
        "Optagelse fra MIDI-keyboard, også i loop.",
        "21 indbyggede bandinstrumenter (Rock, Pop, Jazz) – og dine egne SoundFont-filer (.sf2).",
        "Plugins: VST3, Audio Unit (Mac), LV2 (Linux) og CLAP.",
        "Import og eksport af MIDI og MusicXML (MuseScore, Sibelius, Finale, Dorico).",
        "Fortryd/gentag på alt.",
    ])
    p("Knapper og menuer i programmet er på engelsk. I denne manual står de med <b>fed</b> skrift præcis som i programmet.")

    # ---- 2
    h1("2. Installation")
    h2("Mac")
    steps([
        "Pak zip-filen <b>MIDI-Composer-macOS-Universal.zip</b> ud. Den virker på både Intel- og Apple Silicon-Macs.",
        "Flyt <b>MIDI Composer.app</b> til mappen <b>Programmer</b> (Applications).",
        "Programmet er ikke signeret af Apple, så macOS blokerer det første gang. Åbn Terminal og kør:",
    ])
    code('xattr -dr com.apple.quarantine "/Applications/MIDI Composer.app"')
    p("Siger macOS stadig, at programmet ikke kan åbnes, eller at det \"kan skade din computer\", så giv det lov i "
      "Systemindstillinger:")
    steps([
        "Prøv at åbne <b>MIDI Composer</b> én gang, og luk advarslen (vælg <b>OK</b> eller <b>Færdig</b>, ikke "
        "<b>Flyt til papirkurv</b>).",
        "Åbn <b>Systemindstillinger → Anonymitet og sikkerhed</b>.",
        "Rul ned til afsnittet <b>Sikkerhed</b>. Der står, at \"MIDI Composer\" blev blokeret. Klik <b>Åbn alligevel</b>.",
        "Bekræft med din adgangskode eller Touch ID, og klik <b>Åbn</b> i den næste dialog.",
    ])
    p("Det skal kun gøres én gang pr. version af programmet. Knappen <b>Åbn alligevel</b> vises kun i cirka en time efter, "
      "at du har forsøgt at åbne programmet – kan du ikke se den, så prøv at åbne programmet igen først.")
    tip("På ældre macOS-versioner (14 og tidligere) kan du i stedet højreklikke på programmet, vælge <b>Åbn</b> og "
        "bekræfte. Fra macOS 15 virker det ikke længere – brug Systemindstillinger som beskrevet ovenfor.")
    h2("Windows")
    p("Pak <b>MIDI-Composer-Windows-x64.zip</b> ud i en mappe og start <b>MIDI Composer.exe</b>. Filen "
      "<b>GeneralUser-GS.sf2</b> skal blive liggende i samme mappe som programmet – den indeholder de indbyggede instrumenter.")
    h2("Debian og Ubuntu")
    code("sudo apt install ./midi-composer_<version>_amd64.deb")
    p("Programmet findes derefter i programmenuen som <b>MIDI Composer</b> eller kan startes med kommandoen "
      "<b>midi-composer</b>.")

    # ---- 3
    h1("3. Første start")
    steps([
        "Første gang spørger programmet, om det skal <b>scanne efter instrument-plugins</b>. Har du plugins installeret "
        "(fx Kontakt), så vælg <b>Scan</b>. Du kan altid gøre det senere via <b>Options → Rescan plugins</b>.",
        "Åbn <b>Options → Settings (audio, MIDI, modifier keys)</b> og vælg dit lydkort og dit MIDI-keyboard (se kapitel 13).",
        "Et nyt projekt har ét spor med <b>Grand Piano</b>. Spil på keyboardet, eller klik i piano roll for at sætte toner ind.",
    ])
    tip("Hører du ingenting? Tjek at det rigtige lydkort er valgt i Settings, at sporet ikke er muted (M), og at "
        "lydstyrken på sporet ikke står helt nede.")

    # ---- 4
    h1("4. Vinduet")
    p("Vinduet består af seks dele, oppefra og ned:")
    table(["Del", "Hvad den gør"], [
        ["Menulinje", "<b>File</b> (projekter, import/eksport), <b>Edit</b> (fortryd, kopiér, spor) og <b>Options</b> "
                      "(indstillinger, plugins, lyde, manual, om programmet). På Mac ligger menuen øverst på skærmen."],
        ["Transportlinje", "Afspil, stop, optag, loop, tempo, taktart, nodegrid og fortryd."],
        ["Akkordpanel", "Grundtone, skala, akkordtilstand, modifier-knapper, den aktuelle akkord og forslag til næste akkord."],
        ["Sporliste (venstre)", "Alle spor med navn, farve, mute/solo, lydstyrke, panorering, kanal og instrument."],
        ["Visningslinje", "Over editoren: knappen <b>Step Sequencer</b> skifter mellem piano roll og step sequencer "
                          "(kapitel 9), og <b>Zoom</b>-knapperne zoomer i piano roll."],
        ["Piano roll (højre)", "Tonerne på det aktive spor, med tidslinje øverst, klaviatur til venstre og "
                               "velocity-felt (anslagsstyrke) nederst."],
    ], [38, 127])

    # ---- 5
    h1("5. Transport, tempo og loop")
    table(["Knap / felt", "Funktion"], [
        ["<b>|&lt;</b> (Return to start)", "Hopper til starten – eller til loopets start, hvis loop er slået til. Nodearket følger med."],
        ["<b>Play</b>", "Starter afspilning fra afspilningslinjen."],
        ["<b>Stop</b>", "Stopper. Trykker du <b>Stop</b> igen, mens der er stoppet, hopper linjen til start."],
        ["<b>Rec</b>", "Starter optagelse på det aktive spor (se kapitel 11)."],
        ["<b>Loop</b>", "Slår loop til/fra. Start- og sluttakt sættes i felterne <b>Loop bars</b>."],
        ["Positionsvisning", "Viser takt, slag og tid for afspilningslinjen."],
        ["<b>BPM</b>", "Tempo i slag pr. minut."],
        ["<b>Sig</b>", "Taktart, fx 4/4, 3/4 eller 6/8."],
        ["<b>Grid</b>", "Nodegrid: 1/4, 1/8, 1/16, 1/32 og trioler (1/8 T, 1/16 T)."],
        ["<b>Snap</b>", "Når slået til, lægger nye og flyttede toner sig på nodegridet."],
        ["<b>Undo / Redo</b>", "Fortryd og gentag."],
    ], [45, 120])
    h2("Tidslinjen")
    bullets([
        "<b>Klik</b> i tidslinjen over piano roll for at flytte afspilningslinjen.",
        "<b>Shift+træk</b> eller <b>højre-træk</b> i tidslinjen for at markere et loop-område.",
    ])

    # ---- 6
    h1("6. Spor")
    h2("Tilføj og slet")
    p("Brug <b>Edit → Add track</b> og <b>Edit → Delete active track</b>. Klik på et spor i sporlisten for at gøre det aktivt "
      "– det er det spor, du ser og redigerer i piano roll, og som MIDI-keyboardet spiller på.")
    h2("Indstillinger pr. spor")
    table(["Element", "Funktion"], [
        ["Farveknap", "Den lille firkant ved navnet. Åbner farvevælgeren (se nedenfor)."],
        ["Navn", "Dobbeltklik og skriv for at omdøbe sporet."],
        ["<b>M</b> / <b>S</b>", "Mute (slå lyden fra) og Solo (hør kun dette spor)."],
        ["Kanal", "MIDI-kanal 1–16. Bruges ved eksport og af plugins, der lytter på bestemte kanaler."],
        ["Lydstyrke / Pan", "Skyderne for lydstyrke og panorering (venstre/højre)."],
        ["Instrumentknap", "Viser instrumentets navn. Klik for at vælge instrument (se kapitel 7)."],
        ["<b>E</b> (editor)", "Åbner pluginets eget vindue, hvis det har et (fx Kontakt)."],
    ], [38, 127])
    h2("Farver")
    p("Klik på farveknappen ved et spor for at åbne <b>farvevælgeren</b>:")
    bullets([
        "Træk i farvefeltet og skyderne, eller skriv en hex-værdi. Sporet skifter farve med det samme.",
        "<b>Presets</b>: ti faste farver.",
        "<b>Saved</b>: dine egne swatches. Klik <b>Save swatch</b> for at gemme den aktuelle farve. De gemmes i dine "
        "indstillinger og kan bruges i alle projekter. Højreklik på en swatch for at slette den.",
        "<b>Default</b> nulstiller til standardfarven.",
        "Klik uden for boksen for at lukke den. Farveskiftet kan fortrydes i ét trin.",
    ])
    p("Det aktive spor vises i kraftig farve – både i sporlisten og i piano roll. De andre spor er dæmpede, og deres toner "
      "skinner svagt igennem i piano roll i deres egen farve, som gennem kalkerpapir. Dem kan du se, men ikke redigere.")

    # ---- 7
    h1("7. Instrumenter")
    p("Klik på instrumentknappen på et spor. Menuen har disse grupper:")
    table(["Menu", "Indhold"], [
        ["<b>Internal → Rock / Pop / Jazz</b>", "De indbyggede bandinstrumenter (tabellen nedenfor)."],
        ["<b>Internal → Basic Synth</b>", "En enkel, let synth uden samples."],
        ["<b>SoundFonts</b>", "Dine egne .sf2-filer – én undermenu pr. fil – og <b>Open Sounds folder</b>."],
        ["<b>VST3, AudioUnit, LV2, CLAP</b>", "Dine installerede plugins, når de er scannet."],
        ["<b>Browse plugins...</b>", "Et vindue med søgning og filtrering efter format."],
        ["<b>Bypass</b> / <b>Remove instrument</b>", "Slå instrumentet midlertidigt fra, eller fjern det."],
    ], [55, 110])
    h2("De indbyggede instrumenter")
    table(["Rock", "Pop", "Jazz"], [
        ["Rock Drums", "Pop Drums", "Jazz Drums"],
        ["Distortion Guitar", "Grand Piano", "Brush Drums"],
        ["Overdrive Guitar", "Electric Piano", "Jazz Piano"],
        ["Clean Electric Guitar", "Acoustic Guitar", "Upright Bass"],
        ["Electric Bass (Pick)", "Ukulele", "Jazz Guitar"],
        ["Rock Organ", "Electric Bass (Finger)", "Tenor Sax"],
        ["", "Strings", "Trumpet"],
        ["", "", "Vibraphone"],
    ], [55, 55, 55])
    h2("Trommesæt")
    p("Trommesættene følger General MIDI-standarden, så trommespor fra andre programmer passer direkte. De vigtigste:")
    table(["Tangent", "Lyd", "Tangent", "Lyd"], [
        ["C1", "Stortromme", "F♯1", "Lukket hihat"],
        ["C♯1", "Rimshot / sidestick", "G♯1", "Hihat med pedal"],
        ["D1", "Lilletromme", "A♯1", "Åben hihat"],
        ["D♯1", "Håndklap", "C♯2", "Crash-bækken"],
        ["E1", "Lilletromme 2", "D♯2", "Ride-bækken"],
        ["F1, G1, A1, B1, C2, D2", "Tam-tams (lav → høj)", "F2", "Ride-klokke"],
    ], [28, 54, 28, 55])
    tip("Akkordtilstand bør være slået fra på trommespor – ellers bliver én tangent til tre trommer. Bruger du "
        "standard-modifiertasterne C1/D1 (se kapitel 10), rammer de netop stortromme og lilletromme.")
    h2("Dine egne lyde (SoundFonts)")
    steps([
        "Vælg <b>Options → Open Sounds folder</b>. Mappen åbner i Finder/Stifinder.",
        "Kopiér en eller flere <b>.sf2</b>-filer ind i mappen. Der findes mange gratis SoundFonts på nettet.",
        "Vælg <b>Options → Rescan plugins</b>.",
        "Lydene findes nu under <b>SoundFonts</b> i instrumentmenuen, sorteret efter filnavn.",
    ])
    p("Projekter husker, hvilken fil og lyd hvert spor bruger. Flytter eller sletter du filen, vises sporet som "
      "manglende, indtil filen er tilbage.")
    h2("Plugins")
    p("Programmet kan bruge VST3-plugins på alle systemer, Audio Units på Mac, LV2 på Linux samt CLAP. Efter installation "
      "af nye plugins: <b>Options → Rescan plugins</b>. Går et plugin ned under scanningen, sættes det automatisk på en "
      "sortliste og springes over næste gang.")
    p("Mangler et plugin, som et projekt bruger, står der <b>Plugin missing</b> på sporet. Tonerne er der stadig – vælg et "
      "andet instrument, eller installér pluginet igen.")

    # ---- 8
    h1("8. Piano roll")
    h2("Toner")
    table(["Handling", "Sådan"], [
        ["Indsæt tone", "<b>Klik</b> på et tomt sted. Træk til højre, mens du holder knappen nede, for at bestemme længden. "
                        "I akkordtilstand indsættes en hel akkord."],
        ["Flyt", "Træk i en tone. Op/ned ændrer tonehøjde, til siden ændrer tid."],
        ["Ændr længde", "Træk i tonens <b>højre kant</b> for at ændre, hvor den slutter, eller i den <b>venstre kant</b> "
                        "for at ændre, hvor den starter. Over en kant skifter musemarkøren til en ↔ pil."],
        ["Markér", "Klik på en tone. <b>Shift+klik</b> tilføjer/fjerner fra markeringen."],
        ["Markér flere", "<b>⌘/Ctrl+træk</b> eller <b>Shift+træk</b> på et tomt sted tegner en markeringsboks. "
                         "<b>⌘/Ctrl+A</b> markerer alt."],
        ["Slet én tone", "<b>Højreklik</b> på tonen."],
        ["Slet mange", "Hold <b>højre museknap</b> nede og træk en <b>rød boks</b> over tonerne. Alle toner, boksen rører, "
                       "slettes, når du slipper. Eller markér dem og tryk <b>Delete</b>/<b>Backspace</b>."],
        ["Transponér", "<b>↑ / ↓</b> flytter markerede toner en halvtone. <b>Shift+↑ / ↓</b> flytter en oktav."],
        ["Kopiér / klip / indsæt", "<b>⌘/Ctrl+C</b>, <b>⌘/Ctrl+X</b>, <b>⌘/Ctrl+V</b>. Indsættes ved afspilningslinjen."],
    ], [38, 127])
    p("På en Mac-trackpad svarer højreklik til et klik med to fingre eller Ctrl+klik.")
    h2("Anslagsstyrke (velocity)")
    p("Feltet nederst viser hver tones anslagsstyrke som en lodret streg. Klik og træk hen over stregerne for at ændre den. Er nogle toner markeret, ændres kun de markerede. Bløde "
      "toner vises lidt mørkere i piano roll.")
    h2("Zoom og rul")
    table(["Mus / trackpad", "Virkning"], [
        ["Rul", "Rul op/ned (og til siden med trackpad)."],
        ["Shift+rul", "Rul vandret i tid."],
        ["⌘/Ctrl+rul", "Zoom ind/ud i tid."],
        ["Alt/⌥+rul", "Zoom lodret (højere/lavere tonerækker)."],
        ["Rul i tidslinjen", "Zoom i tid."],
        ["<b>Zoom out / Zoom in</b>", "Knapper i visningslinjen: zoom i tid. Tasterne <b>+</b> og <b>-</b> gør det samme."],
        ["<b>Lower / Taller</b>", "Knapper i visningslinjen: lavere eller højere tonerækker. Klaviaturet følger med."],
    ], [45, 120])
    h2("Klaviaturet")
    p("Klik på tangenterne til venstre for at høre tonerne. Tonerne i den valgte skala er fremhævet som lysere rækker i "
      "piano roll, så du let kan se, hvilke toner der passer.")

    # ---- 9
    h1("9. Step sequencer")
    p("Step sequenceren er en hurtig måde at bygge trommerytmer og andre gentagne mønstre på. En step sequencer "
      "indsættes som et spor og bygges op af <b>linjer</b>. Hver linje spiller én tone med sit eget instrument, fx "
      "stortromme, lilletromme og hi-hat, eller en bastone på et basinstrument.")
    h2("Kom i gang")
    steps([
        "Klik <b>Step Sequencer</b> i linjen over editoren (eller tryk <b>S</b>). Piano roll skiftes ud med step "
        "sequenceren. Har projektet ingen endnu, indsættes en med en <b>Kick</b>-linje på Pop Drums.",
        "Klik <b>+ Line</b> for at tilføje flere linjer. Nye linjer foreslår Snare, Closed Hat, Open Hat, Clap osv. og "
        "bruger samme instrument som linjen over.",
        "Klik på trinnene for at tænde dem. Tryk <b>mellemrum</b> for at afspille – det aktuelle trin har en rød ramme.",
    ])
    p("<b>+ New step sequencer</b> (eller <b>Edit → Add step sequencer track</b>) indsætter endnu en step sequencer. "
      "Vælg hvilken der vises i listen ved siden af titlen. Klik <b>Step Sequencer</b> igen for at komme tilbage til piano roll.")
    h2("En linje")
    table(["Del", "Hvad den gør"], [
        ["Navn", "Dobbeltklik for at omdøbe linjen."],
        ["Instrumentknap", "Vælg linjens instrument – samme menu som i sporlisten (kapitel 7)."],
        ["Tone", "Den tone, linjen spiller. For trommesæt: C1 stortromme, D1 lilletromme, F♯1 lukket hi-hat, A♯1 åben hi-hat."],
        ["<b>M</b>", "Slå linjen fra (mute)."],
        ["<b>X</b>", "Slet linjen (kan fortrydes)."],
        ["Trin", "<b>Klik</b> = til/fra. <b>Shift+klik</b> = accent (kraftigere). <b>Højreklik</b> = fra. "
                 "<b>Træk</b> hen over flere trin for at tænde eller slukke dem alle."],
    ], [38, 127])
    h2("Indstillinger for hele step sequenceren")
    table(["Indstilling", "Virkning"], [
        ["<b>Steps</b>", "Antal trin i mønsteret (4–64)."],
        ["<b>Step</b>", "Længden af hvert trin: 1/4, 1/8, 1/16, 1/32 eller trioler. 16 trin à 1/16 = én takt i 4/4."],
        ["<b>Repeat</b>", "Hvor mange gange mønsteret spilles."],
        ["<b>Start bar</b>", "Den takt, step sequenceren starter i."],
    ], [38, 127])
    tip("Hver linje er også et almindeligt spor i sporlisten med lydstyrke, panorering, mute/solo og farve. Tonerne "
        "laves ud fra mønsteret, så de afspilles, eksporteres til MIDI og vises i piano roll som alle andre toner. "
        "Ret dem i step sequenceren – ændringer i piano roll erstattes, næste gang mønsteret ændres.")

    # ---- 10
    h1("10. Akkordtilstand")
    p("Akkordtilstand gør, at hver tangent – på MIDI-keyboardet eller i piano roll – giver en hel akkord, der passer i den "
      "valgte skala. Spiller du C i C-dur, får du C-dur-akkorden. Spiller du D, får du D-mol, og så videre.")
    h2("Sådan slår du det til")
    steps([
        "Vælg grundtone i <b>Root</b> og skala i <b>Scale</b> (fx A Natural Minor).",
        "Klik <b>Chord Mode</b>, så den viser ON – eller tryk <b>C</b> på computerens tastatur.",
        "Spil eller klik en tone i skalaen. Toner uden for skalaen spilles som enkelttoner.",
    ])
    p("Skalaer: Major, Natural Minor, Harmonic Minor, Melodic Minor, Dorian, Phrygian, Lydian, Mixolydian og Locrian.")
    h2("Modifiers: omvendinger, 7'ere og 9'ere")
    p("Med modifier-knapperne i akkordpanelet ændrer du akkorden. Du kan klikke på knapperne (de bliver ved med at være slået "
      "til), eller holde de tilsvarende tangenter nede på keyboardet:")
    table(["Modifier", "Standardtangent", "Virkning"], [
        ["<b>1st inversion</b>", "C1", "Den øverste tone flyttes ned under den tone, du spiller."],
        ["<b>2nd inversion</b>", "D1", "De to øverste toner flyttes ned under den tone, du spiller."],
        ["<b>7</b>", "C♯1", "Tilføjer septimen (fx Am → Am7, G → G7)."],
        ["<b>9</b>", "D♯1", "Tilføjer septim og none (fx Am → Am9)."],
    ], [35, 32, 98])
    p("Den tone, du spiller, bliver altid liggende, hvor du satte den. Eksempel med C-dur spillet fra C3:")
    table(["", "Toner"], [
        ["Grundstilling", "C3 – E3 – G3"],
        ["1st inversion", "G2 – C3 – E3"],
        ["2nd inversion", "E2 – G2 – C3"],
    ], [45, 120])
    p("Modifier-tasterne spiller ikke selv lyd i akkordtilstand. Du kan vælge andre tangenter i Settings (kapitel 13). "
      "Den aktuelle akkord vises i akkordpanelet, fx <b>Am7 – 1st inversion</b>.")
    h2("Forslag til næste akkord")
    p("Når du har spillet en akkord, viser akkordpanelet op til fire forslag ved <b>Next</b>. Klik på et forslag for at "
      "høre det. Hold musen over det for at se, hvorfor det er foreslået. Forslagene følger kvintcirklen inden for skalaen, "
      "med det stærkeste først:")
    table(["Prioritet", "Grundtonens bevægelse", "Eksempel i C-dur"], [
        ["1", "En kvint ned (stærkeste opløsning)", "G → C, Dm → G"],
        ["2", "En kvint op", "C → G"],
        ["3", "En terts ned (to fælles toner)", "C → Am"],
        ["4", "Tilbage til grundtonen", "→ C"],
        ["5", "Et trin op", "C → Dm"],
    ], [22, 78, 65])

    # ---- 10
    h1("11. Optagelse")
    steps([
        "Vælg det spor, du vil optage på.",
        "Tryk <b>Rec</b> (eller <b>R</b>). Afspilningen starter, og det, du spiller, vises med rødt.",
        "Tryk <b>Stop</b> (eller <b>mellemrum</b>). Tonerne lægges ind på sporet i ét trin, som kan fortrydes.",
    ])
    p("Er <b>Loop</b> slået til, kører optagelsen rundt i loopet, og alt, hvad du spiller på de forskellige gennemløb, "
      "kommer med. På den måde kan du bygge lag på lag, fx først bas og så akkorder. Akkordtilstand og modifiers virker også "
      "under optagelse.")

    # ---- 11
    h1("12. Filer, import og eksport")
    table(["Menu", "Funktion"], [
        ["<b>File → New / Open / Save / Save as</b>", "Projekter gemmes som <b>.mcproj</b>. Spor, toner, instrumenter med "
                                                        "deres indstillinger, farver, tempo og loop kommer med."],
        ["<b>File → Import MIDI file...</b>", "Lægger sporene fra en .mid-fil ind i projektet."],
        ["<b>File → Export MIDI file...</b>", "Gemmer alle spor som en standard MIDI-fil, der kan åbnes i alle musikprogrammer."],
        ["<b>File → Import MusicXML (MuseScore)...</b>", "Indlæser .musicxml, .xml eller .mxl. Én stemme bliver ét spor."],
        ["<b>File → Export MusicXML (MuseScore)...</b>", "Gemmer noder, som kan åbnes i MuseScore, Sibelius, Finale og Dorico."],
    ], [62, 103])
    h2("MusicXML – til og fra noder")
    p("<b>Eksport</b>: toneart (fra Root/Scale), taktart og tempo kommer med. Tonerne afrundes til nærmeste 1/16, toner "
      "over taktstreger bindes, og akkorder skrives som akkorder. Klaverspor med stort toneomfang får to systemer.")
    p("<b>Import</b>: noder, akkorder, pauser, overbindinger, optakter og flere stemmer kommer med, og programmet sætter "
      "Root/Scale efter partiturets toneart. Gentagelser foldes ikke ud, og forslagstoner og dynamik springes over.")
    p("I MuseScore: brug <b>File → Export → MusicXML</b> (ikke den almindelige .mscz-fil) for at få noderne over i MIDI Composer.")

    # ---- 12
    h1("13. Indstillinger")
    p("<b>Options → Settings</b> indeholder:")
    bullets([
        "<b>Lydkort</b>: udgang, samplerate og <b>bufferstørrelse</b>. En lille buffer (128–256 samples) giver kort "
        "forsinkelse, når du spiller. Hører du knas, så vælg en større.",
        "<b>MIDI input</b>: dit MIDI-keyboard. Programmet forbinder automatisk igen, hvis keyboardet tages ud og sættes i.",
        "<b>Chord modifier keys</b>: hvilke tangenter der virker som 1st inversion, 2nd inversion, 7 og 9. Klik <b>MIDI Learn</b> "
        "og tryk på den tangent, du vil bruge. <b>Defaults</b> sætter standardtangenterne C1, D1, C♯1 og D♯1 tilbage.",
    ])
    p("<b>Options → User manual</b> åbner manualen på engelsk (den danske manual kan hentes fra projektets Releases-side "
      "på GitHub). <b>Options → About MIDI Composer</b> viser version og credits.")

    # ---- 13
    h1("14. Tastaturgenveje")
    table(["Tast", "Funktion"], [
        ["Mellemrum", "Afspil / stop"],
        ["Home", "Til start (eller loopets start)"],
        ["R", "Optag til/fra"],
        ["L", "Loop til/fra"],
        ["C", "Akkordtilstand til/fra"],
        ["S", "Piano roll / step sequencer"],
        ["+ / -", "Zoom ind / ud i tid (piano roll)"],
        ["⌘/Ctrl+Z", "Fortryd"],
        ["⌘/Ctrl+Shift+Z eller ⌘/Ctrl+Y", "Gentag"],
        ["⌘/Ctrl+N / O / S", "Nyt projekt / Åbn / Gem"],
        ["⌘/Ctrl+Shift+S", "Gem som"],
        ["⌘/Ctrl+A", "Markér alle toner"],
        ["⌘/Ctrl+C / X / V", "Kopiér / klip / indsæt toner"],
        ["Delete / Backspace", "Slet markerede toner"],
        ["↑ / ↓", "Transponér markerede toner en halvtone"],
        ["Shift+↑ / ↓", "Transponér markerede toner en oktav"],
    ], [65, 100])
    p("⌘ gælder på Mac, Ctrl på Windows og Linux.")

    # ---- 14
    h1("15. Fejlfinding")
    table(["Problem", "Løsning"], [
        ["Ingen lyd", "Tjek lydkortet i Settings, mute/solo og lydstyrke på sporet, og at sporet har et instrument."],
        ["Lyden kommer for sent", "Vælg en mindre bufferstørrelse i Settings (fx 256 eller 128 samples)."],
        ["Lyden knaser", "Vælg en større bufferstørrelse, eller brug færre tunge plugins på én gang."],
        ["MIDI-keyboardet gør ingenting", "Vælg det under MIDI input i Settings. Tjek at det rigtige spor er aktivt."],
        ["Én tangent giver tre toner", "Akkordtilstand er slået til – tryk <b>C</b> eller klik <b>Chord Mode</b>."],
        ["En tangent spiller ingen lyd", "Den er sat som modifier-tast i akkordtilstand. Slå akkordtilstand fra eller vælg "
                                          "andre modifier-tangenter i Settings."],
        ["Et plugin mangler i listen", "Kør <b>Options → Rescan plugins</b>. Plugins, der gik ned under scanningen, springes over."],
        ["Egne .sf2-lyde vises ikke", "Filen skal ligge i Sounds-mappen og ende på .sf2. Kør derefter <b>Rescan plugins</b>."],
        ["Mac: \"kan ikke åbnes\"", "Kør kommandoen fra kapitel 2 (xattr), eller gå til <b>Systemindstillinger → "
                                    "Anonymitet og sikkerhed</b> og klik <b>Åbn alligevel</b> (se kapitel 2)."],
        ["De indbyggede instrumenter mangler", "Filen GeneralUser-GS.sf2 skal ligge ved programmet (Windows/Linux) "
                                               "eller i appen (Mac). Installér programmet igen."],
    ], [55, 110])

    # ---- 15
    h1("16. Credits og licenser")
    bullets([
        "<b>GeneralUser GS</b> – de indbyggede instrumenter – af S. Christian Collins. "
        "https://www.schristiancollins.com/generaluser. Licensen følger med som GeneralUser-GS-LICENSE.txt.",
        "<b>TinySoundFont</b> – afspilning af SoundFonts – af Bernhard Schelling (MIT-licens). "
        "https://github.com/schellingb/TinySoundFont",
        "<b>JUCE</b> – programbiblioteket, MIDI Composer er bygget på. https://juce.com",
        "Kildekode: https://github.com/shenphenchoedron-hue/easymidieditor",
    ])



# ================================================================== build
class Doc(SimpleDocTemplate):
    def afterFlowable(self, f):
        if hasattr(f, "_toc"):
            key = f"h{id(f)}"
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(f._toc, key, level=0)
            self.notify("TOCEntry", (0, f._toc, self.page, key))


FOOTER = {"left": "", "page": ""}


def footer(canvas, doc):
    if doc.page == 1:
        return
    canvas.saveState()
    canvas.setFont(BODY, 8.5)
    canvas.setFillColor(colors.HexColor("#888888"))
    canvas.drawString(20 * mm, 12 * mm, FOOTER["left"])
    canvas.drawRightString(190 * mm, 12 * mm, f"{FOOTER['page']} {doc.page}")
    canvas.setStrokeColor(GRID)
    canvas.line(20 * mm, 16 * mm, 190 * mm, 16 * mm)
    canvas.restoreState()


def render(content, out, title, footer_left, page_word):
    story.clear()
    content()
    FOOTER["left"], FOOTER["page"] = footer_left, page_word
    doc = Doc(out, pagesize=A4, leftMargin=22 * mm, rightMargin=22 * mm, topMargin=20 * mm, bottomMargin=22 * mm,
              title=title, author="MIDI Composer")
    doc.multiBuild(list(story), onFirstPage=footer, onLaterPages=footer)
    print("Wrote", out)


if __name__ == "__main__":
    import sys
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    from manual_en import content_en
    me = sys.modules[__name__]
    render(lambda: content_en(me), OUT_EN, "MIDI Composer - User Manual", "MIDI Composer – User Manual", "Page")
    render(content_da, OUT_DA, "MIDI Composer – Brugermanual", "MIDI Composer – Brugermanual", "Side")
