# Agent notes

Para agentes (Cursor u otros):

1. Leer `.cursor/rules/` (sobre todo `esp32-lab.mdc` y `new-view-slide.mdc`) y `docs/ARCHITECTURE.md`.
2. No commitear secretos (`docs/SECURITY.md`).
3. **View nueva = slide + carpeta `src/views/<nombre>/` + README** (ver checklist en `new-view-slide.mdc`).
4. Build: `pio run` / flash: `pio run -t upload`.
5. Idioma de respuesta al usuario: español (si el usuario escribe en español).
