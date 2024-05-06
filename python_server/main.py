from args import parse_args
from app import start

if __name__ == '__main__':
    args = parse_args()
    start("Dash App", args.ip, args.output_dir, args.data_dir, args.history_len, args.min_distances)
