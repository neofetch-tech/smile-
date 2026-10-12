# smile assets

Put the application logo here as `logo.png` (the smile face).

Once the file is in place, the build picks it up automatically
(see `app.qrc` and `src/frontend/CMakeLists.txt`):
it becomes the window icon and the launcher banner.
If the file is missing, the app falls back to a text title,
so the build never breaks.
