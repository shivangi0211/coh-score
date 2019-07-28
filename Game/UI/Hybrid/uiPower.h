#ifndef UIPOWER_H
#define UIPOWER_H

void resetPowerMenu();
void powerMenu();

typedef struct BasePowerSet BasePowerSet;
const BasePowerSet * g_pPrimaryPowerSet;
const BasePowerSet * g_pSecondaryPowerSet;

void makeValidCharacter();

int powersReady(int redirect);
int powersChosen(int redirect);

void powerMenuEnterCode();
void powerMenuExitCode();

#endif