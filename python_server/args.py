import os, argparse

def parse_args():
    dir_path = os.path.dirname(os.path.realpath(__file__)).replace("\\", "/")

    parser = argparse.ArgumentParser(prog="server", description="Processing and visualization of data pulled from ESP Master device")
    parser.add_argument("--ip",
                        default="192.168.4.1",
                        help="IP of the Master device")
    parser.add_argument("--save_dir", default=dir_path + "/data")
    return parser.parse_args()
