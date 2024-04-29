from args import parse_args
from app import start

import plotly.express as px

if __name__ == '__main__':
    args = parse_args()
    start("Dash App", args.ip)
