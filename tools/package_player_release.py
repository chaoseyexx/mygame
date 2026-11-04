import os
import zipfile
import shutil

def package_player_client():
    dist_dir = 'dist'
    os.makedirs(dist_dir, exist_ok=True)

    player_zip_path = os.path.join(dist_dir, 'Mercenary_Release_v0.0.1.zip')
    player_zip_path_generic = os.path.join(dist_dir, 'Mercenary_Release.zip')
    player_root_zip = 'Mercenary_Release.zip'

    print("Building Clean Player Client Game Archive (NO Server, NO Editor)...")

    # Files to include in player release
    core_files = [
        'MercenaryGame.exe',
        'steam_api64.dll',
        'steam_appid.txt',
        'phonon.dll'
    ]

    with zipfile.ZipFile(player_zip_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        # Add core binaries
        for f in core_files:
            if os.path.exists(f):
                print(f"  + {f}")
                zf.write(f, f)
            else:
                print(f"  ! Warning: {f} not found in root")

        # Add assets
        for root, dirs, files in os.walk('assets'):
            for file in files:
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, '.')
                archive_name = rel_path.replace('\\', '/')
                zf.write(full_path, archive_name)

    print(f"Player release built successfully: {player_zip_path} ({os.path.getsize(player_zip_path) / (1024*1024):.2f} MB)")

    # Copy to generic names for launcher local fallbacks
    shutil.copyfile(player_zip_path, player_zip_path_generic)
    shutil.copyfile(player_zip_path, player_root_zip)
    print(f"Mirrored to: {player_zip_path_generic} and {player_root_zip}")

    # Build Clean Turnkey Dedicated Server Package (Zero external scripts)
    server_zip_path = os.path.join(dist_dir, 'Mercenary_DevTools_v1.1.zip')
    print("\nBuilding Clean All-in-One Turnkey Dedicated Server Package (NO batch files, NO vbs)...")
    server_files = [
        'MercenaryServer.exe',
        'MercenaryEditor.exe',
        'steam_api64.dll',
        'steam_appid.txt',
        'phonon.dll'
    ]
    with zipfile.ZipFile(server_zip_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for f in server_files:
            if os.path.exists(f):
                print(f"  + {f}")
                zf.write(f, f)
        # Include all maps
        for root, dirs, files in os.walk('assets/maps'):
            for file in files:
                full_path = os.path.join(root, file)
                archive_name = full_path.replace('\\', '/')
                zf.write(full_path, archive_name)

    print(f"Dedicated Server built successfully: {server_zip_path} ({os.path.getsize(server_zip_path) / (1024*1024):.2f} MB)")

if __name__ == '__main__':
    package_player_client()
