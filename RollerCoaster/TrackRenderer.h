#pragma once

#include <GL/glew.h>

// inicijalizacija shadera, VAO/VBO za prugu i stubove
// - ocekuje da je vec inicijalizovan OpenGL kontekst
void initTrackRenderer();

// crtanje pruge i stubova
void renderTrack();

// oslobadjanje resursa
void cleanupTrackRenderer();
