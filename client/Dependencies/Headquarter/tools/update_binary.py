import argparse
import download
import dump_key
import dump_area_info
import inflate
import os
import print_gw_exe_file_id
from scanner import FileScanner
import sys
import utils

try:
    from tqdm import tqdm
except:
    print("Install tqdm 'pip install tqdm'")
    sys.exit(1)

def get_path_from_workspace(workspace, *fragments):
    if workspace is None:
        return utils.get_path(*fragments)
    else:
        return os.path.join(workspace, *fragments)

def main(args):
    exe_path = get_path_from_workspace(args.workspace, 'Gw.exe')
    if os.path.isfile(exe_path):
        scanner = FileScanner(exe_path)
        file_id = print_gw_exe_file_id.get_file_id(scanner)
        #file_id = 0
    else:
        file_id = 0
    
    client = download.FileClient()
    client.connect()
   

    if (file_id == 0) or (client.file_id_latest_exe != file_id):
        fr = client.request_file(client.file_id_latest_exe)
        with tqdm(total=fr.size_compressed, unit='B') as progress:
            for percent in fr.download():
                progress.update(fr.last_chunk)

        # The scanner holds reference to `exe_path` preventing it from being overwritten.
        if 'scanner' in globals():
            del scanner
        if os.path.isfile(exe_path):
          os.unlink(exe_path)
        with open(exe_path, 'wb') as fd:
            fd.write(fr.decompressed())
        scanner = FileScanner(exe_path)

    pr, pm, pk = dump_key.get_keys_from_scanner(scanner)
    build = dump_key.get_build_number(scanner)
    output = get_path_from_workspace(args.workspace, 'data', f'gw_{build}.pub')
    print(f"Writing Diffie-Hellman keys to '{output}'")
    dump_key.write_keys_in_file(output, pr, pm, pk)

    build_file_path = get_path_from_workspace(args.workspace, 'Gw.build')
    print(f"Writing build number to '{build_file_path}'")
    open(build_file_path, 'w').write(f'{build}')
    
    area_info_file = get_path_from_workspace(args.workspace, 'code', 'client', 'data', f'area_info.data')
    area_info = dump_area_info.get_area_info_from_scanner(scanner)
    open(area_info_file, 'w').write(f'{area_info}')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("--workspace", type=str, required=False,
        help="Workspace in order to define where to write the file")
    args = parser.parse_args()
    main(args)
