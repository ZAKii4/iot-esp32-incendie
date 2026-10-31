#!/usr/bin/env python3
"""
attaques.py - Banc d'essai offensif du validateur (tests "red team")
====================================================================

Joue le role d'un attaquant qui envoie au validateur des JSON fabriques pour :
  - le faire PLANTER (deni de service, exception non geree),
  - le faire ACCEPTER un JSON qui ne respecte pas le modele (contournement),
  - INJECTER du contenu dangereux (terminal, CSV, dashboard).

Chaque attaque est envoyee a "python3 valider.py -" dans un processus separe,
avec un delai maximum. Une attaque est REUSSIE (= faille) si :
  - le validateur repond VALIDE alors que le JSON est malveillant,
  - ou il plante (code de sortie anormal, trace Python),
  - ou il depasse le delai (blocage),
  - ou sa sortie contient des caracteres de controle (injection terminal).

Usage :
    python3 attaques.py                  # teste le valider.py du meme dossier
    python3 attaques.py /chemin/dossier  # teste un autre valider.py
"""

import copy
import json
import subprocess
import sys
from pathlib import Path

DOSSIER = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent
VALIDEUR = DOSSIER / "valider.py"
DELAI_S = 10

BASE = {
    "status": {"temperature": 25.5, "light": 3095, "regul": "RUNNING", "fire": False,
               "heat": "OFF", "cold": "OFF", "fanspeed": 0},
    "location": {"room": "Salle TP IoT", "gps": {"lat": 33.5731, "lon": -7.5898},
                 "address": "EMSI Casablanca - site CFC"},
    "regul": {"lt": 22, "ht": 26},
    "info": {"ident": "ESP32 CDCF8C", "user": "Malak Zaoufy & Zakaria Khamlich",
             "loc": "EMSI Casablanca"},
    "net": {"uptime": "2", "ssid": "NOP", "mac": "BC:DD:C2:CD:CF:8C", "ip": "0.0.0.0"},
    "reporthost": {"target_ip": "127.0.0.1", "target_port": 1880, "sp": 2},
}
TEXTE_BASE = json.dumps(BASE)


def modif(chemin, valeur):
    """Copie de BASE ou le champ 'a/b/c' recoit 'valeur' (ou est supprime si valeur is SUPPR)."""
    d = copy.deepcopy(BASE)
    *parents, cle = chemin.split("/")
    cible = d
    for p in parents:
        cible = cible[p]
    if valeur is SUPPR:
        del cible[cle]
    else:
        cible[cle] = valeur
    return json.dumps(d, ensure_ascii=False)


def remplacer(ancien, nouveau):
    """Remplacement textuel brut dans le JSON de base (pour fabriquer du JSON non standard)."""
    assert ancien in TEXTE_BASE, ancien
    return TEXTE_BASE.replace(ancien, nouveau, 1)


SUPPR = object()

# (categorie, nom, charge utile en str ou bytes)
ATTAQUES = [
    # ---------------- Deni de service ----------------
    ("DoS", "fichier geant (20 Mo)", '{"x":"' + "A" * 20_000_000 + '"}'),
    ("DoS", "imbrication profonde [[[[...]]]] x 100 000", "[" * 100_000 + "]" * 100_000),
    ("DoS", "imbrication profonde {\"a\":{...}} x 50 000", '{"a":' * 50_000 + "1" + "}" * 50_000),
    ("DoS", "entier a 100 000 chiffres", remplacer('"light": 3095', '"light": ' + "9" * 100_000)),
    ("DoS", "chaine de 5 Mo dans un champ autorise", modif("info/user", "x" * 5_000_000)),
    ("DoS", "regex : MAC de 1 Mo", modif("net/mac", "AB:" * 350_000)),

    ("DoS", "imbrication 7 000 niveaux, sous la limite de taille", "[" * 7_000 + "]" * 7_000),
    ("DoS", "imbrication d'objets 2 500 niveaux, sous la limite", '{"a":' * 2_500 + "1" + "}" * 2_500),
    ("DoS", "6 niveaux seulement mais hors modele", '{"status":{"a":{"b":{"c":{"d":{"e":1}}}}}}'),
    ("DoS", "entier de 5 000 chiffres, sous la limite", remplacer('"light": 3095', '"light": ' + "7" * 5_000)),
    ("DoS", "reel de 5 000 chiffres, sous la limite", remplacer('"lt": 22', '"lt": 0.' + "1" * 5_000)),
    ("DoS", "1 500 cles dans un objet, sous la limite",
     '{' + ",".join(f'"k{i}":0' for i in range(1500)) + '}'),

    # ---------------- Nombres pieges ----------------
    ("Nombre", "1e400 (devient +infini) dans un seuil", remplacer('"lt": 22', '"lt": 1e400')),
    ("Nombre", "-1e400 (devient -infini) dans un seuil", remplacer('"ht": 26', '"ht": -1e400')),
    ("Nombre", "NaN", remplacer('"temperature": 25.5', '"temperature": NaN')),
    ("Nombre", "Infinity", remplacer('"temperature": 25.5', '"temperature": Infinity')),
    ("Nombre", "seuil absurde 1e300", modif("regul/ht", 1e300)),
    ("Nombre", "entier ecrit en reel : light 3095.0", remplacer('"light": 3095', '"light": 3095.0')),
    ("Nombre", "entier ecrit 1e3 : port 1.88e3", remplacer('"target_port": 1880', '"target_port": 1.88e3')),
    ("Nombre", "fanspeed gigantesque", modif("status/fanspeed", 10**18)),
    ("Nombre", "sp = 0 seconde", modif("reporthost/sp", 0)),
    ("Nombre", "sp gigantesque", modif("reporthost/sp", 10**12)),

    # ---------------- Confusion de types ----------------
    ("Type", "fire = 1 au lieu de true", modif("status/fire", 1)),
    ("Type", "light = true", modif("status/light", True)),
    ("Type", "temperature = \"25\" (chaine)", modif("status/temperature", "25")),
    ("Type", "temperature = [25]", modif("status/temperature", [25])),
    ("Type", "status = null", modif("status", None)),
    ("Type", "racine = tableau", json.dumps([BASE])),
    ("Type", "racine = chaine", json.dumps(TEXTE_BASE)),
    ("Type", "racine = null", "null"),

    # ---------------- Contournement du schema ----------------
    ("Schema", "MAC suivie d'un retour a la ligne (bug du $ en regex)", modif("net/mac", "BC:DD:C2:CD:CF:8C\n")),
    ("Schema", "uptime suivi d'un retour a la ligne", modif("net/uptime", "42\n")),
    ("Schema", "IP suivie d'un retour a la ligne", modif("net/ip", "0.0.0.0\n")),
    ("Schema", "chaine composee uniquement d'espaces", modif("info/ident", "   ")),
    ("Schema", "ssid de 10 000 caracteres", modif("net/ssid", "S" * 10_000)),
    ("Schema", "champ en trop cache dans gps", modif("location/gps/alt", 12)),
    ("Schema", "cle __proto__ (pollution de prototype JS)", modif("__proto__", {"admin": True})),
    ("Schema", "cle en double via echappement unicode \\u0069",
     remplacer('"light": 3095', '"light": 3095, "l\\u0069ght": 1')),
    ("Schema", "cle sosie en cyrillique (lіght)", remplacer('"light"', '"lіght"')),
    ("Schema", "champ obligatoire absent", modif("status/heat", SUPPR)),

    # ---------------- Injections ----------------
    ("Injection", "sequence ANSI qui efface le terminal dans ident",
     modif("info/ident", "\u001b[2J\u001b[31mPWNED")),
    ("Injection", "sequence ANSI dans une CLE inconnue", modif("status/\u001b[2Jx", 1)),
    ("Injection", "octet nul \\u0000 dans user", modif("info/user", "Malak\u0000admin")),
    ("Injection", "retour chariot (faux log) dans room", modif("location/room", "312\r\nVALIDE   faux.json")),
    ("Injection", "formule CSV =cmd|' /C calc'!A0 dans address", modif("location/address", "=cmd|' /C calc'!A0")),
    ("Injection", "HTML/JS <script> dans user", modif("info/user", "<script>alert(1)</script>")),
    ("Injection", "inversion bidi U+202E dans ident", modif("info/ident", "ESP32 ‮LIVE")),
    ("Injection", "caractere invisible U+200B dans ident", modif("info/ident", "ESP32​")),
    ("Injection", "demi-surrogate isole \\ud800", remplacer('"NOP"', '"\\ud800"')),

    # ---------------- Encodage et syntaxe ----------------
    ("Encodage", "UTF-16", TEXTE_BASE.encode("utf-16")),
    ("Encodage", "octets invalides UTF-8 (0xFF)", TEXTE_BASE.encode()[:-2] + b"\xff}"),
    ("Encodage", "BOM UTF-8 en tete", b"\xef\xbb\xbf" + TEXTE_BASE.encode()),
    ("Encodage", "deux JSON a la suite", TEXTE_BASE + TEXTE_BASE),
    ("Encodage", "commentaire // dans le JSON", TEXTE_BASE.replace("{", "{ // pirate\n", 1)),
    ("Encodage", "vide", ""),
    ("Encodage", "uniquement des espaces", "   \n\t  "),

    # ---------------- Coherence metier ----------------
    ("Coherence", "chauffage ET clim allumes", json.dumps({**BASE, "status": {**BASE["status"], "heat": "ON", "cold": "ON"}})),
    ("Coherence", "regulation RUNNING sans temperature", modif("status/temperature", None)),
]

# Temoins : des JSON legitimes qui DOIVENT rester acceptes
TEMOINS = [
    ("JSON de l'ESP", TEXTE_BASE),
    ("capteur absent (null + HALT)", json.dumps({**BASE, "status": {**BASE["status"], "temperature": None, "regul": "HALT"}})),
    ("version jolie sur plusieurs lignes", json.dumps(BASE, indent=2)),
    ("accents dans un texte", modif("location/address", "École EMSI - Maârif, Casablanca, Maroc")),
    ("modele du prof (corrige)", json.dumps({**BASE, "status": {**BASE["status"], "regul": "HALT", "fire": True, "fanspeed": 1234},
                                             "location": {"room": "312", "gps": {"lat": 43.62453842, "lon": 7.050628185}, "address": "Les lucioles"},
                                             "regul": {"lt": 26, "ht": 25.89999962},
                                             "info": {"ident": "ESP32 123", "user": "GM", "loc": "A Biot"},
                                             "net": {"uptime": "55", "ssid": "Livebox-B870", "mac": "AC:67:B2:37:C9:48", "ip": "192.168.1.45"}})),
    ("temperature negative", modif("status/temperature", -10.25)),
]


def lancer(charge):
    donnees = charge if isinstance(charge, bytes) else charge.encode("utf-8", "surrogatepass")
    try:
        r = subprocess.run([sys.executable, str(VALIDEUR), "-"], input=donnees,
                           capture_output=True, timeout=DELAI_S)
    except subprocess.TimeoutExpired:
        return "BLOCAGE", ""
    sortie = r.stdout.decode("utf-8", "replace") + r.stderr.decode("utf-8", "replace")
    if "Traceback" in sortie or r.returncode not in (0, 1):
        return "PLANTAGE", sortie
    if any(ord(c) < 32 and c not in "\n\t" or 0x7F <= ord(c) < 0xA0 for c in sortie):
        return "INJECTION_SORTIE", sortie
    return ("ACCEPTE" if r.returncode == 0 else "REJETE"), sortie


def main():
    print(f"Cible : {VALIDEUR}\n")
    failles = 0
    for cat, nom, charge in ATTAQUES:
        res, sortie = lancer(charge)
        ok = (res == "REJETE")
        failles += not ok
        motif = sortie.strip().splitlines()[1].strip()[:70] if ok and len(sortie.strip().splitlines()) > 1 else ""
        print(f"{'  bloquee' if ok else 'X FAILLE '} [{cat:9}] {nom:55} -> {res} {motif}")
    print()
    faux_rejets = 0
    for nom, charge in TEMOINS:
        res, sortie = lancer(charge)
        ok = (res == "ACCEPTE")
        faux_rejets += not ok
        print(f"{'  accepte' if ok else 'X REJETE A TORT'} [Temoin   ] {nom:55} -> {res}")
    print(f"\nBilan : {len(ATTAQUES) - failles}/{len(ATTAQUES)} attaques bloquees, "
          f"{len(TEMOINS) - faux_rejets}/{len(TEMOINS)} JSON legitimes acceptes.")
    return 0 if failles == 0 and faux_rejets == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
