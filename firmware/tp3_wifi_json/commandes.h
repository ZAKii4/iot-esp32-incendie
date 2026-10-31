/*
 * commandes.h - Parametrage a chaud via le moniteur serie
 *   (9600 baud, fin de ligne = "Nouvelle ligne")
 *
 *   SB 22     seuil bas (C)            SH 26     seuil haut (C)
 *   SF 30     seuil temperature feu    D 800     ecart de lumiere "feu"
 *   T 35      simule une temperature   T OFF     revient au capteur
 *   CAL       recalibre la lumiere     ?         affiche les reglages
 */
#pragma once

void lireCommandes();
void afficherReglages();
