import numpy as np
import pandas as pd

import dash
from dash import Dash, html, dcc, Input, Output, State, callback
from plotly import graph_objects as go

from data import Memory
from master_comm import get_data, force_advertise

class GlobalData:
    app: dash.Dash
    ip: str
    memory: Memory = Memory()

g_data = GlobalData()

def start(app_name: str, ip: str):
    global g_data

    g_data.ip = ip
    g_data.app = dash.Dash(app_name)
    g_data.app.layout = html.Div([
        html.P(id='dummy'),
        dcc.Tabs(
            id='tabs', value='live-update-tab', children=[
                dcc.Tab(label='Live', children=[
                    html.Div([
                        dcc.Input(id='in-master-ip', type='text', placeholder='Master IPv4', value=g_data.ip),
                        html.Button('Set', id='submit-master-ip', n_clicks=0),
                        dcc.Graph(id='live-scatter3d'),
                        dcc.Interval(
                            id='graph-update-interval',
                            interval=3000 # ms
                        )
                    ])
                ]),
                dcc.Tab(label='History', children=[
                    dcc.Graph(id='history-scatter3d')
                ])
        ]),
        dcc.Interval(
            id='data-update-interval',
            interval=3000 # ms
        )
    ])
    g_data.app.run(debug=True)

def update_data():
    global g_data
    data = get_data(g_data.ip)
    if not data:
        return

    # Insert new data
    g_data.memory.update(data)
    # Remove old
    g_data.memory.remove_old_devices()
    # Update scanner positions
    g_data.memory.update_scanner_positions()
    # Update device positions
    g_data.memory.update_device_positions()
    # Check for advertising
    scanner = g_data.memory.get_scanner_to_advertise()
    if scanner:
        force_advertise(g_data.ip, scanner)
        print(f"{scanner} should advertise")
    for mac, v in g_data.memory.scanner_dict.items():
        print(mac, v)

@callback(Output('live-scatter3d', 'figure'), [Input('graph-update-interval', 'n_intervals')])
def update_live_graph(_):
    xs,ys,zs = [], [], []
    data_s = []
    for mac, sc in g_data.memory.scanner_dict.items():
        if sc.has_position():
            xs.append(sc.x)
            ys.append(sc.y)
            zs.append(sc.z)
            data_s.append(f"{mac}")
    xd,yd,zd = [], [], []
    data_d = []
    for mac, dev in g_data.memory.device_dict.items():
        if dev.has_position():
            xd.append(dev.x)
            yd.append(dev.y)
            zd.append(dev.z)
            data_d.append(f"{mac}")

    fig = go.Figure()
    fig.add_scatter3d(
        x=xs, y=ys, z=zs,
        mode='markers', marker=dict(
            size=12, color='rgb(255,0,0)',
            opacity=0.75
        ),
        customdata=data_s,
        hovertemplate="<b>X</b>: %{x}<br><b>Y</b>: %{y}<br><b>Z</b>: %{z}<br>%{customdata}"
    )
    fig.add_scatter3d(
        x=xd, y=yd, z=zd,
        mode='markers', marker=dict(
            size=12, color='rgb(0,255,0)',
            opacity=0.75
        ),
        customdata=data_d,
        hovertemplate="<b>X</b>: %{x}<br><b>Y</b>: %{y}<br><b>Z</b>: %{z}<br>%{customdata}"
    )
    fig.update_layout(
        autosize=True,
        width=1280,
        height=720,
        scene=dict(
            xaxis=dict(range=[-15,15]),
            yaxis=dict(range=[-15,15]),
            zaxis=dict(range=[-15,15]),
        )
    )
    fig.update_layout(uirevision='constant')
    return fig

#@callback(Output(''), Input('live-scatter3d, clickData'))
#def live_graph_click(click_data):
#    pass

@callback(Output('history-scatter3d', 'figure'), [Input('graph-update-interval', 'n_intervals')])
def update_history_graph(_):
    pass

@callback(Output('dummy', 'hidden'), [Input('data-update-interval', 'n_intervals')])
def get_master_data(_):
    update_data()
    return True

@callback(
        Output('in-master-ip', 'value'),
        Input('submit-master-ip', 'n_clicks'),
        State('in-master-ip', 'value'),
        prevent_initial_call=True
)
def set_ip(n_clicks: int, value: str):
    global g_data
    g_data.ip = value
    print(f"Updated ip: '{g_data.ip}'")
