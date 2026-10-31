#!/usr/bin/env python3
"""
valider.py - Validateur durci du status JSON de l'ESP32
=======================================================

Usage :
    python3 valider.py fichier.json [autre.json ...]
    python3 valider.py exemples/*.json
    cat status.json | python3 valider.py -        (lecture sur l'entree standard)
    python3 valider.py --strict fichier.json      (les avertissements deviennent des erreurs)

Code de sortie : 0 si TOUS les fichiers sont valides, 1 sinon.

Principe de securite : "tout ce qui n'est pas explicitement autorise est refuse",
et en cas de doute ou d'erreur interne, on REFUSE (fail-closed).

Le validateur verifie, dans l'ordre :
  1. LECTURE   : fichier regulier, taille <= 16 Kio (lue AVANT de tout charger),
                 UTF-8 strict, sans BOM
  2. SYNTAXE   : JSON strict (RFC 8259) : refuse NaN/Infinity, nombres infinis (1e400),
                 entiers geants, cles en double, imbrication > 5 niveaux (verifiee
                 AVANT l'analyse : une "bombe d'imbrication" n'est jamais construite)
  3. SECURITE  : aucune chaine (cle ou valeur) ne contient de caractere de controle,
                 invisible, d'inversion bidi ou de demi-surrogate
  4. SCHEMA    : esp_status.schema.json, avec des types STRICTS
                 (3095.0 n'est pas un entier, true n'est pas un nombre)
  5. COHERENCE : regles metier que JSON Schema ne sait pas exprimer

Tout ce qui est affiche est "neutralise" : un JSON piege ne peut pas envoyer de
sequences de controle au terminal de l'operateur.

La fonction valider_texte() / valider_octets() est reutilisable dans le serveur du TP5.
Dependance : jsonschema >= 4
"""

import json
import math
import os
import re
import stat
import sys
import unicodedata
from pathlib import Path

try:
    import jsonschema
except ImportError:
    sys.exit("Il manque la bibliotheque jsonschema.\n"
             f"Installe-la pour CE Python ({sys.executable}) :\n"
             "    python3 -m pip install jsonschema")
try:
    from jsonschema import Draft202012Validator     # existe depuis jsonschema 4.0
    from jsonschema.validators import extend
except ImportError:
    sys.exit(f"jsonschema {getattr(jsonschema, '__version__', '?')} est trop ancien "
             "(il faut la version 4 ou plus).\n"
             "    python3 -m pip install --upgrade jsonschema")

SCHEMA_PATH = Path(__file__).with_name("esp_status.schema.json")

TAILLE_MAX = 16 * 1024      # octets : un status reel fait ~550 octets (~1 Kio en "joli")
CHIFFRES_MAX = 15           # un entier JSON de plus de 15 chiffres est refuse
NOMBRE_MAX_CAR = 32         # un nombre ecrit sur plus de 32 caracteres est refuse
PROFONDEUR_MAX = 5          # le modele n'a que 3 niveaux (racine > location > gps)
BOM = b"\xef\xbb\xbf"


class Rejet(Exception):
    """Toute raison de refuser le JSON. Le message est destine a l'humain."""

    def __init__(self, niveau, message):
        super().__init__(message)
        self.niveau = niveau


# ============================================================================
# Affichage neutralise : rien de ce qui vient du JSON n'atteint le terminal brut
# ============================================================================
def propre(texte, maxi=80):
    """Echappe tout caractere non imprimable (\\x1b, \\u202e...) et tronque."""
    texte = str(texte)
    sortie = []
    for c in texte:
        if c.isprintable() and not 0xD800 <= ord(c) <= 0xDFFF:
            sortie.append(c)
        else:
            sortie.append(c.encode("unicode_escape", "backslashreplace").decode("ascii")
                          if ord(c) < 0x10000 else f"\\U{ord(c):08x}")
    texte = "".join(sortie)
    return texte if len(texte) <= maxi else texte[:maxi - 3] + "..."


# ============================================================================
# 1. LECTURE
# ============================================================================
def lire_octets(source):
    """Lit au plus TAILLE_MAX + 1 octets : impossible de saturer la memoire."""
    if source == "-":
        return sys.stdin.buffer.read(TAILLE_MAX + 1)
    try:
        infos = os.stat(source)
    except OSError:
        raise Rejet("LECTURE", f"fichier introuvable ou illisible : {propre(source)}")
    if not stat.S_ISREG(infos.st_mode):          # refuse /dev/zero, dossiers, tubes...
        raise Rejet("LECTURE", "ce n'est pas un fichier ordinaire")
    if infos.st_size > TAILLE_MAX:
        raise Rejet("LECTURE", f"fichier trop gros ({infos.st_size} octets > {TAILLE_MAX})")
    try:
        with open(source, "rb") as f:
            return f.read(TAILLE_MAX + 1)
    except OSError:
        raise Rejet("LECTURE", f"lecture impossible : {propre(source)}")


def decoder(octets):
    if len(octets) > TAILLE_MAX:
        raise Rejet("LECTURE", f"contenu trop gros (> {TAILLE_MAX} octets)")
    if octets.startswith(BOM):
        raise Rejet("LECTURE", "BOM UTF-8 en tete : interdit en JSON (RFC 8259, section 8.1)")
    if b"\x00" in octets:
        raise Rejet("LECTURE", "octet nul : ce n'est pas du texte UTF-8 (UTF-16 ?)")
    try:
        return octets.decode("utf-8", errors="strict")
    except UnicodeDecodeError as e:
        raise Rejet("LECTURE", f"le contenu n'est pas de l'UTF-8 valide (octet {e.start})")


# ============================================================================
# 2. SYNTAXE (json de Python, rendu strict par des "crochets")
# ============================================================================
def _refuser_constante(nom):
    raise Rejet("SYNTAXE", f"'{nom}' n'existe pas en JSON standard (RFC 8259)")


def _entier(s):
    if len(s.lstrip("-")) > CHIFFRES_MAX:
        raise Rejet("SYNTAXE", f"entier de {len(s.lstrip('-'))} chiffres : trop long")
    return int(s)


def _reel(s):
    if len(s) > NOMBRE_MAX_CAR:
        raise Rejet("SYNTAXE", "nombre ecrit sur trop de caracteres")
    x = float(s)
    if not math.isfinite(x):                     # 1e400 -> inf : refuse
        raise Rejet("SYNTAXE", f"nombre hors limites (devient infini) : {propre(s)}")
    return x


def _refuser_doublons(paires):
    vues = {}
    for cle, valeur in paires:
        if cle in vues:
            raise Rejet("SYNTAXE", f"cle en double : \"{propre(cle)}\"")
        vues[cle] = valeur
    return vues


def verifier_profondeur(texte):
    """Compte les { [ hors des chaines AVANT d'analyser : une bombe d'imbrication
    ([[[[...]]]] x 10 000) est refusee sans jamais etre construite en memoire."""
    profondeur, dans_chaine, echappe = 0, False, False
    for c in texte:
        if dans_chaine:
            if echappe:
                echappe = False
            elif c == "\\":
                echappe = True
            elif c == '"':
                dans_chaine = False
        elif c == '"':
            dans_chaine = True
        elif c in "[{":
            profondeur += 1
            if profondeur > PROFONDEUR_MAX:
                raise Rejet("SYNTAXE", f"imbrication trop profonde (plus de {PROFONDEUR_MAX} niveaux)")
        elif c in "]}":
            profondeur -= 1


def analyser(texte):
    """Texte JSON -> objet Python, ou Rejet explicite. Ne leve JAMAIS autre chose."""
    if not texte.strip():
        raise Rejet("SYNTAXE", "le contenu est vide")
    verifier_profondeur(texte)
    try:
        return json.loads(texte, parse_constant=_refuser_constante, parse_int=_entier,
                          parse_float=_reel, object_pairs_hook=_refuser_doublons)
    except Rejet:
        raise
    except RecursionError:
        raise Rejet("SYNTAXE", "imbrication beaucoup trop profonde")
    except json.JSONDecodeError as e:
        lignes = texte.splitlines() or [""]
        ligne = lignes[e.lineno - 1] if 0 < e.lineno <= len(lignes) else ""
        msg = traduire_json(e.msg)
        if re.search(r"[:\[,]\s*-?0$", ligne[:e.colno - 1]) and ligne[e.colno - 1:e.colno].isdigit():
            msg = "nombre commencant par 0 (ex : 0123) : interdit en JSON, ecrire 123"
        debut = max(0, e.colno - 40)
        extrait = propre(ligne[debut:e.colno + 20], 70)
        curseur = " " * len(propre(ligne[debut:e.colno - 1], 70)) + "^"
        raise Rejet("SYNTAXE", f"JSON invalide ligne {e.lineno}, colonne {e.colno} : {msg}\n"
                               f"        {extrait}\n        {curseur}")
    except (ValueError, MemoryError, TypeError) as e:
        raise Rejet("SYNTAXE", f"contenu non analysable ({type(e).__name__})")


def traduire_json(msg):
    table = {
        "Expecting ',' delimiter": "virgule ',' manquante",
        "Expecting ':' delimiter": "deux-points ':' manquant",
        "Expecting value": "valeur attendue (nombre avec un 0 devant ? virgule en trop ? guillemet manquant ?)",
        "Expecting property name enclosed in double quotes":
            "nom de cle attendu entre guillemets doubles (virgule en trop avant '}' ?)",
        "Extra data": "contenu en trop apres la fin du JSON",
        "Illegal trailing comma": "virgule en trop avant '}' ou ']'",
        "Unterminated string starting at": "chaine de caracteres non fermee",
        "Invalid control character at": "caractere de controle brut dans une chaine",
        "Invalid \\escape": "sequence d'echappement invalide",
        "Invalid \\uXXXX escape": "sequence \\uXXXX invalide",
    }
    for anglais, francais in table.items():
        if msg.startswith(anglais):
            return francais
    return propre(msg)


# ============================================================================
# 3. SECURITE : inspection de TOUTES les chaines (cles et valeurs)
# ============================================================================
CATEGORIES_INTERDITES = {
    "Cc": "caractere de controle",           # \x00, \x1b (ANSI), \r, \n, \t...
    "Cf": "caractere invisible / bidi",      # U+200B, U+202E, U+FEFF...
    "Cs": "demi-surrogate isole",            # \ud800 : UTF-16 casse
    "Co": "caractere a usage prive",
    "Cn": "caractere non attribue",
    "Zl": "separateur de ligne",             # U+2028
    "Zp": "separateur de paragraphe",        # U+2029
}


def verifier_chaine(s, ou):
    for i, c in enumerate(s):
        cat = unicodedata.category(c)
        if cat in CATEGORIES_INTERDITES or (cat == "Zs" and c != " "):
            nom = CATEGORIES_INTERDITES.get(cat, "espace speciale")
            raise Rejet("SECURITE", f"{ou} : {nom} U+{ord(c):04X} en position {i} "
                                    f"(\"{propre(s, 40)}\")")


def inspecter(valeur, chemin="(racine)"):
    if isinstance(valeur, str):
        verifier_chaine(valeur, chemin)
    elif isinstance(valeur, dict):
        for cle, v in valeur.items():
            verifier_chaine(cle, f"{chemin} (nom de cle)")
            inspecter(v, cle if chemin == "(racine)" else f"{chemin}/{cle}")
    elif isinstance(valeur, list):
        for i, v in enumerate(valeur):
            inspecter(v, f"{chemin}[{i}]")


# ============================================================================
# 4. SCHEMA, avec des types stricts
# ============================================================================
def _est_entier(_, x):
    return isinstance(x, int) and not isinstance(x, bool)          # 3095.0 refuse


def _est_nombre(_, x):
    return (isinstance(x, (int, float)) and not isinstance(x, bool)
            and (isinstance(x, int) or math.isfinite(x)))


TypesStricts = extend(
    Draft202012Validator,
    type_checker=Draft202012Validator.TYPE_CHECKER.redefine_many(
        {"integer": _est_entier, "number": _est_nombre}),
)


def charger_validateur(chemin=SCHEMA_PATH):
    schema = json.loads(Path(chemin).read_text(encoding="utf-8"))
    Draft202012Validator.check_schema(schema)      # le schema lui-meme doit etre correct
    return TypesStricts(schema)


VALIDATEUR = charger_validateur()


def chemin_lisible(erreur):
    morceaux = [propre(p, 30) for p in erreur.absolute_path]
    return "/".join(morceaux) if morceaux else "(racine)"


def apercu(valeur):
    try:
        return propre(json.dumps(valeur, ensure_ascii=False), 40)
    except (TypeError, ValueError):
        return "?"


def traduire_schema(e):
    v = e.validator
    if v == "required":
        manquant = e.message.split("'")[1] if "'" in e.message else e.message
        return f"champ obligatoire manquant : \"{propre(manquant)}\""
    if v == "additionalProperties":
        props = set(e.schema.get("properties", {}))
        en_trop = sorted(set(e.instance) - props) if isinstance(e.instance, dict) else []
        return "champ(s) non prevu(s) par le modele : " + ", ".join(f'"{propre(c, 30)}"' for c in en_trop[:5])
    if v == "type":
        return f"mauvais type : {apercu(e.instance)} alors que le type attendu est {e.validator_value}"
    if v == "enum":
        return f"valeur {apercu(e.instance)} interdite, valeurs permises : {e.validator_value}"
    if v == "pattern":
        return f"format ou caracteres non autorises : {apercu(e.instance)}"
    if v == "minimum":
        return f"valeur {apercu(e.instance)} trop petite (minimum {e.validator_value})"
    if v == "maximum":
        return f"valeur {apercu(e.instance)} trop grande (maximum {e.validator_value})"
    if v == "minLength":
        return "chaine vide interdite"
    if v == "maxLength":
        return f"chaine trop longue ({len(e.instance)} caracteres, maximum {e.validator_value})"
    if v == "anyOf":
        return f"valeur {apercu(e.instance)} invalide (attendu : nombre entre -55 et 125, ou null)"
    return propre(e.message)


def valider_schema(donnees):
    erreurs = sorted(VALIDATEUR.iter_errors(donnees), key=lambda e: list(map(str, e.absolute_path)))
    return [f"{chemin_lisible(e)} : {traduire_schema(e)}" for e in erreurs[:20]]


# ============================================================================
# 5. COHERENCE metier
# ============================================================================
def verifier_coherence(d):
    erreurs, avertissements = [], []
    st, rg = d["status"], d["regul"]

    if st["heat"] == "ON" and st["cold"] == "ON":
        erreurs.append("status : radiateur (heat) ET climatisation (cold) allumes en meme temps")
    if st["temperature"] is None and st["regul"] == "RUNNING":
        erreurs.append("status : regulation RUNNING alors qu'aucune temperature n'est mesuree")
    if rg["lt"] >= rg["ht"]:
        avertissements.append(f"regul : seuil bas lt={rg['lt']} >= seuil haut ht={rg['ht']}")
    if st["fire"] and (st["heat"] == "ON" or st["fanspeed"] > 0):
        avertissements.append("status : incendie detecte mais chauffage ou ventilation encore actifs")
    return erreurs, avertissements


# ============================================================================
# API principale (reutilisable dans un serveur) - FAIL-CLOSED
# ============================================================================
def _valider(texte, strict):
    donnees = analyser(texte)
    inspecter(donnees)

    erreurs = [f"SCHEMA : {m}" for m in valider_schema(donnees)]
    if erreurs:
        return False, erreurs, []

    err_coh, avert = verifier_coherence(donnees)
    erreurs = [f"COHERENCE : {m}" for m in err_coh]
    avert = [f"ATTENTION : {m}" for m in avert]
    if strict:
        erreurs, avert = erreurs + avert, []
    return not erreurs, erreurs, avert


def valider_texte(texte, strict=False):
    """Renvoie (valide, erreurs, avertissements). Ne leve jamais d'exception."""
    try:
        if not isinstance(texte, str):
            raise Rejet("LECTURE", "le contenu doit etre une chaine")
        if len(texte.encode("utf-8", "surrogatepass")) > TAILLE_MAX:
            raise Rejet("LECTURE", f"contenu trop gros (> {TAILLE_MAX} octets)")
        return _valider(texte, strict)
    except Rejet as r:
        return False, [f"{r.niveau} : {r}"], []
    except Exception as e:                       # fail-closed : dans le doute, on refuse
        return False, [f"INTERNE : erreur inattendue ({type(e).__name__}) -> JSON refuse"], []


def valider_octets(octets, strict=False):
    try:
        return valider_texte(decoder(octets), strict)
    except Rejet as r:
        return False, [f"{r.niveau} : {r}"], []
    except Exception as e:
        return False, [f"INTERNE : erreur inattendue ({type(e).__name__}) -> JSON refuse"], []


def main(arguments):
    strict = "--strict" in arguments
    sources = [a for a in arguments if a != "--strict"]
    if not sources:
        print(__doc__)
        return 1

    tout_valide = True
    for source in sources:
        try:
            ok, erreurs, avert = valider_octets(lire_octets(source), strict)
        except Rejet as r:
            ok, erreurs, avert = False, [f"{r.niveau} : {r}"], []
        except Exception as e:
            ok, erreurs, avert = False, [f"INTERNE : {type(e).__name__} -> JSON refuse"], []

        print(f"{'VALIDE  ' if ok else 'INVALIDE'} {propre(source, 120)}")
        for m in erreurs + avert:
            # dernier filet : chaque ligne affichee est neutralisee
            print("    - " + "\n".join(propre(l, 200) for l in m.split("\n")))
        tout_valide &= ok

    return 0 if tout_valide else 1


if __name__ == "__main__":
    sys.setrecursionlimit(1000)
    sys.exit(main(sys.argv[1:]))
