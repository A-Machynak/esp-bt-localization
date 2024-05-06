import os, argparse

def parse_args():
    dir_path = os.path.dirname(os.path.realpath(__file__)).replace("\\", "/")

    parser = argparse.ArgumentParser(prog="server", description="Processing and visualization of data pulled from ESP Master device")
    parser.add_argument("--ip",
                        default="192.168.4.1",
                        help="IP of the Master device")
    parser.add_argument("--output_dir",
                        default=dir_path + "/data",
                        help="Where to put exported snapshots.")
    parser.add_argument("--data_dir",
                        default=dir_path + "/data",
                        help="Where to put configuration data (saved MAC addresses).")
    parser.add_argument("--history_len",
                        default=10240, type=int,
                        help="How many snapshots (created every 3 seconds) to save before exporting them.")
    return parser.parse_args()
