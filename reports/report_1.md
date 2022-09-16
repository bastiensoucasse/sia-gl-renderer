# Advanced OpenGL Rendering

## 1. Defered Shading

L'objectif est d'éviter les calculs inutiles (fragments non visibles) en procédant à un rendu en deux étapes.

### 1.1. G-buffer

Tout d'abord, l'objet FBO est initialisé de manière à stocker les deux textures (couleurs et normales), ainsi que la profondeur.
