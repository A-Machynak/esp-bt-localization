import threading, json, time, base64
from pathlib import Path
from datetime import datetime

import dash
from dash import dcc, html, Input, Output, State, callback
from plotly import graph_objects as go

from common import Mac
from data import Device, Memory, Scanner
from snapshot import Snapshot
from master_comm import get_data, force_advertise
from algorithm import DEFAULT_ENV_FACTOR, DEFAULT_REF_PATH_LOSS

class GlobalData:
    app: dash.Dash
    ip: str
    memory: Memory = Memory()
    current_history: list[Snapshot] = []
    selected_mac: Mac = None
    line_history: int = 10
    mtx: threading.Lock = threading.Lock()
    output_dir: str # Snapshots output dir
    data_dir: str   # Data config dir
    config_data: dict[Mac, tuple[str, int, float]] = {}

g_data = GlobalData()

def start(app_name: str, ip: str, output_dir: str, data_dir: str, snapshot_limit: int, min_distances: int):
    print(f"app: {app_name}, ip: {ip}, out: '{output_dir}', data: '{data_dir}', len: {snapshot_limit}")
    global g_data

    g_data.ip = ip
    g_data.memory.snapshot_limit = snapshot_limit
    g_data.memory.min_distances = min_distances
    g_data.output_dir = output_dir
    g_data.data_dir = data_dir
    g_data.config_data = load_config_data(data_dir)

    reading_thread = threading.Thread(target=update_data_task)
    reading_thread.daemon = True
    reading_thread.start()

    PADDING_STYLE = {'padding-left': '1em', 'padding-right': '1em', 'margin': '0'}
    g_data.app = dash.Dash(app_name)
    g_data.app.layout = html.Div([
        html.P(id='dummy'),
        dcc.Tabs(id='tabs', value='live-update-tab', children=[
            dcc.Tab(label='Live', id='live-update-tab', value='live-update-tab', children=[
                dcc.Interval(id='graph-update-interval', interval=3000),
                html.Hr(),
                html.Div([
                    html.P("Master IP", style=PADDING_STYLE),
                    dcc.Input(id='in-master-ip', type='text', placeholder='Master IP', value=g_data.ip, style={'width': '25%'}),
                    html.Button('Set', id='submit-master-ip', n_clicks=0)
                ]),
                html.Hr(),
                html.Div([
                    dcc.Graph(id='live-graph3d', figure={}, style={'display': 'inline-block'}),
                    dcc.Graph(id='live-graph2d', figure={}, style={'display': 'inline-block'})
                ]),
                html.Hr(),
                html.Div([], id='detail-table-live'),
                html.Hr(),
                html.Div([
                    html.H2("Add known device", style=PADDING_STYLE),
                    dcc.Input(id='in-known-device-mac', type='text', placeholder='Device MAC', style={'width':'15%'}),
                    dcc.Input(id='in-known-device-name', type='text', placeholder='Device Name', style={'width':'15%'}),
                    dcc.Input(id='in-known-device-env-factor', type='text', placeholder=f'Env Factor (optional, {str(DEFAULT_ENV_FACTOR)})', style={'width':'15%'}),
                    dcc.Input(id='in-known-device-ref-pl', type='text', placeholder=f'Ref Path Loss (optional, {str(DEFAULT_REF_PATH_LOSS)})', style={'width':'15%'}),
                ]),
                html.Button('Add', id='submit-known-device', n_clicks=0),
                html.Button('Remove', id='submit-remove-known-device', n_clicks=0),
                html.Div([dcc.Textarea(value=f'{get_config_data_str()}', id='known-device-textarea', readOnly=True, placeholder='Saved devices', style={'width':'600px', 'height': '80px'})]),
                html.Button('Reset Scanner Positions', id='submit-reset-scanners-pos', n_clicks=0),
                html.Button('Reset Scanner Positions (use 2D)', id='submit-reset-scanners-pos2d', n_clicks=0),
                html.Button('Reset Scanner Distances', id='submit-reset-scanners-dist', n_clicks=0),
            ]),
            dcc.Tab(label='History', id='history-tab', value='history-tab', children=[
                html.Hr(),
                html.H1('-', id='history-description-label'),
                html.H1('-', id='history-time-label'),
                html.Div([
                    dcc.Graph(id='history-graph3d', figure={}, style={'display': 'inline-block'}),
                    dcc.Graph(id='history-graph2d', figure={}, style={'display': 'inline-block'}),
                ]),
                dcc.Slider(0, 1, 1, value=1, id='history-time-slider', marks=None, tooltip={"always_visible": True, "template": "{value}"}),
                html.Div([], id='detail-table-history'),
                html.Hr(),
                html.Table([
                    html.Tr([
                        html.Td([
                            html.H2('Load'),
                            dcc.Upload(html.Button('Select a File'), id='upload-history-file')
                        ]),
                        html.Td([
                            html.H2('Export'),
                            html.Div([
                                html.P('Data range', style=PADDING_STYLE),
                                dcc.RangeSlider(0, 0, 1, value=[0,0], id='export-time-slider', marks=None, tooltip={"always_visible": True, "template": "{value}"}),
                                html.P('', id='export-data-time-start'),
                                html.P('', id='export-data-time-end'),
                                html.Button('Export data', id='export-data-btn'),
                                dcc.Download(id='export-data-download')
                            ])
                        ])
                    ])
                ], style={'width': '100%'})
            ]),
        ]),
    ], style={'data-bs-theme': "dark"})
    # Don't use debug; the data will be updated twice as often
    g_data.app.run(debug=False)

def update_data_task():
    """Task for pulling data from API."""
    global g_data
    ITER_COUNT = 3
    SLEEP_SEC = 4

    iter = 0
    while True:
        iter += 1
        time.sleep(SLEEP_SEC)
        with g_data.mtx:
            update_data()
            # Check for advertising once in a while
            if iter >= ITER_COUNT:
                iter = 0
                scanner = g_data.memory.get_scanner_to_advertise()
                if scanner:
                    force_advertise(g_data.ip, scanner)
                    print(f"{scanner} should advertise")

def update_data() -> bool:
    global g_data
    data = get_data(g_data.ip)
    if not data:
        return False
    # Insert new data
    g_data.memory.update(data)
    # Remove old
    g_data.memory.remove_old_data()
    # Update scanner positions
    g_data.memory.update_scanner_positions()
    # Update device positions
    g_data.memory.update_device_positions()
    # Save data
    g_data.memory.create_snapshot()

    if len(g_data.memory.snapshots) >= g_data.memory.snapshot_limit:
        filename = f"export_{datetime.now().strftime('%Y%m%d_%H%M%S')}.pkl"
        out = str(Path(g_data.output_dir) / filename)
        Snapshot.export(g_data.memory.snapshots, out)
        g_data.memory.snapshots.clear()
    return True

@callback([
    Output('live-graph3d', 'figure'), Output('live-graph2d', 'figure')],
    Input('graph-update-interval', 'n_intervals'))
def update_live_graph(_):
    """Updating 2D/3D scatter live data."""
    fig3d = go.Figure()
    fig2d = go.Figure()

    # Center point to use as a reference
    center_scatter_figure(fig2d, fig3d)

    with g_data.mtx: # Keep it locked until the end
        # Scanners
        posSc = [[], [], []]
        data = []
        for mac, sc in g_data.memory.scanner_dict.items():
            if sc.has_position():
                posSc[0].append(sc.x)
                posSc[1].append(sc.y)
                posSc[2].append(sc.z)
                if sc.mac in g_data.config_data:
                    data.append(f"{g_data.config_data[sc.mac][0]} - {str(sc.mac)}")
                else:
                    data.append(str(sc.mac))
        scanners_scatter_figure(fig2d, fig3d, posSc, data)

        # Devices
        pos0 = [[], [], []] # Known
        pos1 = [[], [], []] # Public
        pos2 = [[], [], []] # Private
        data0 = []
        data1 = []
        data2 = []
        for mac, dev in g_data.memory.device_dict.items():
            if dev.has_position():
                if mac in g_data.config_data: # Known device
                    pos0[0].append(dev.x)
                    pos0[1].append(dev.y)
                    pos0[2].append(dev.z)
                    data0.append(f"{g_data.config_data[mac][0]} - {str(mac)}")
                elif dev.is_public_addr: # Public
                    pos1[0].append(dev.x)
                    pos1[1].append(dev.y)
                    pos1[2].append(dev.z)
                    data1.append(str(mac))
                else: # Private
                    pos2[0].append(dev.x)
                    pos2[1].append(dev.y)
                    pos2[2].append(dev.z)
                    data2.append(str(mac))
        devices_scatter_figure(fig2d, fig3d, pos0, pos1, pos2, data0, data1, data2)

        # History line
        if g_data.selected_mac is not None:
            # Show last N positions
            pos = [[], [], []]
            for i in range(0, g_data.line_history):
                idx = len(g_data.memory.snapshots) - 1 - i
                if idx < 0:
                    break
                data: Snapshot = g_data.memory.snapshots[idx]
                found = data.find(g_data.selected_mac)
                if not found:
                    break
                pos[0].append(found.x)
                pos[1].append(found.y)
                pos[2].append(found.z)
            if len(pos[0]) > 0:
                devices_line_figure(fig2d, fig3d, pos)

        setup_figure_layout(fig2d, fig3d)
    return fig3d, fig2d


@callback([
    Output('history-graph3d', 'figure'), Output('history-graph2d', 'figure'), Output('history-time-label', 'children')],
    Input('history-time-slider', 'value'))
def update_history_graph(value):
    """Updating 2D/3D scatter for historical data."""

    fig3d = go.Figure()
    fig2d = go.Figure()

    # Center point to use as a reference
    center_scatter_figure(fig2d, fig3d)

    if len(g_data.current_history) <= 0 or value >= len(g_data.current_history):
        return fig3d, fig2d, ''
    data: Snapshot = g_data.current_history[value]

    # Reading historical data, so we can't parse it the same way as live data
    # Scanners
    posSc = [[], [], []]
    data_s = []
    for sc in data.scanners:
        posSc[0].append(sc.x)
        posSc[1].append(sc.y)
        posSc[2].append(sc.z)
        if sc.mac in g_data.config_data:
            data_s.append(f"{g_data.config_data[sc.mac][0]} - {str(sc.mac)}")
        else:
            data_s.append(str(sc.mac))
    scanners_scatter_figure(fig2d, fig3d, posSc, data_s)

    # Devices
    pos0 = [[], [], []] # Known
    pos1 = [[], [], []] # Public
    pos2 = [[], [], []] # Private
    data_d0 = []
    data_d1 = []
    data_d2 = []
    for dev in data.devices:
        if dev.mac in g_data.config_data: # Known device
            pos0[0].append(dev.x)
            pos0[1].append(dev.y)
            pos0[2].append(dev.z)
            data_d0.append(f"{g_data.config_data[dev.mac][0]} - {str(dev.mac)}")
        elif dev.is_public_addr: # Public
            pos1[0].append(dev.x)
            pos1[1].append(dev.y)
            pos1[2].append(dev.z)
            data_d1.append(str(dev.mac))
        else: # Private
            pos2[0].append(dev.x)
            pos2[1].append(dev.y)
            pos2[2].append(dev.z)
            data_d2.append(str(dev.mac))
    devices_scatter_figure(fig2d, fig3d, pos0, pos1, pos2, data_d0, data_d1, data_d2)

    # History line
    if g_data.selected_mac is not None:
        # Show last N positions
        pos = [[], [], []]
        for i in range(0, g_data.line_history):
            if (value - i) < 0:
                break
            data: Snapshot = g_data.current_history[value - i]
            found = data.find(g_data.selected_mac)
            if not found:
                break
            pos[0].append(found.x)
            pos[1].append(found.y)
            pos[2].append(found.z)
        if len(pos[0]) > 0:
            devices_line_figure(fig2d, fig3d, pos)

    setup_figure_layout(fig2d, fig3d)
    return fig3d, fig2d, data.timestamp.strftime("%Y/%m/%d, %H:%M:%S")


@callback([
    Output('detail-table-live', 'children'), Output('live-graph3d', 'clickData'), Output('live-graph2d', 'clickData'),
    Output('in-known-device-mac', 'value'), Output('in-known-device-name', 'value'), Output('in-known-device-env-factor', 'value'),
    Output('in-known-device-ref-pl', 'value')],
    [Input('live-graph2d', 'clickData'), Input('live-graph3d', 'clickData')],
    prevent_initial_call = True
)
def live_graph_click(cd2d, cd3d):
    """Showing detailed data for a clicked point."""

    global g_data
    selected = cd3d if cd3d is not None else cd2d
    if selected is None:
        return html.Table(), None, None
    mac_str = ""
    if 'customdata' in selected['points'][0]:
        mac_str = selected['points'][0]['customdata'][-17:]
    else:
        return html.Table(), None, None, '', '', ''

    mac = Mac.from_str(mac_str)
    with g_data.mtx:
        found = g_data.memory.find(mac)
        if not found:
            print(f'{mac} not found?')
            return html.Table(), None, None
    g_data.selected_mac = found.mac
    if isinstance(found, Scanner):
        table = make_detail_table(True, mac_str, found.x, found.y, found.z)
    else:
        table = make_detail_table(False, mac_str, found.x, found.y, found.z, found.is_ble,
                                  found.is_public_addr, found.event_type, found.advertising_data)
    
    # For 'Add Known Device'
    name = ''
    ef = DEFAULT_ENV_FACTOR
    pl = DEFAULT_REF_PATH_LOSS
    if mac in g_data.config_data:
        name = g_data.config_data[mac][0]
        ef = g_data.config_data[mac][1]
        pl = g_data.config_data[mac][2]

    # We have to reset the 'clickData' for next call by setting it to None...
    return table, None, None, mac_str, name, str(float(ef)), str(int(pl))


@callback([
    Output('detail-table-history', 'children'), Output('history-graph3d', 'clickData'), Output('history-graph2d', 'clickData')],
    [Input('history-graph2d', 'clickData'), Input('history-graph3d', 'clickData')],
    State('history-time-slider', 'value')
)
def history_graph_click(cd2d, cd3d, slider_value):
    """Showing detailed data for a clicked point."""

    global g_data
    selected = cd3d if cd3d is not None else cd2d
    if selected is None or (slider_value < 0 or slider_value >= len(g_data.current_history)):
        return html.Table(), None, None
    mac_str = ""
    if 'customdata' in selected['points'][0]:
        mac_str = selected['points'][0]['customdata'][-17:]
    else:
        return html.Table(), None, None
    mac = Mac.from_str(mac_str)

    found = g_data.current_history[slider_value].find(mac)
    if not found:
        return html.Table(), None, None
    g_data.selected_mac = found.mac

    if isinstance(found, Snapshot.ScannerData):
        table = make_detail_table(True, mac_str, found.x, found.y, found.z)
    else:
        table = make_detail_table(False, mac_str, found.x, found.y, found.z, found.is_ble,
                                  found.is_public_addr, found.event_type, found.advertising_data)
    return table, None, None

@callback([
    Output('history-time-slider', 'min'), Output('history-time-slider', 'max'), Output('history-time-slider', 'value'),
    Output('export-time-slider', 'min'), Output('export-time-slider', 'max'), Output('export-time-slider', 'value'),
    Output('upload-history-file', 'contents'), Output('history-description-label', 'children')],
    [Input('tabs', 'value'), Input('upload-history-file', 'contents'), Input('upload-history-file', 'filename')])
def load_historical_data(tab_name, contents, filename):
    """Switching to history tab or loading data from file. Will invoke `update_history_graph`."""
    global g_data
    description = ''
    if contents is not None:
        # Importing historical data from file
        _, content_string = contents.split(',')
        decoded = base64.b64decode(content_string)
        data = Snapshot.deserialize(decoded)
        if data is not None:
            g_data.current_history = data
            description = filename
    elif tab_name == 'history-tab':
        # Copy snapshots; so they don't get updated while the user is playing with it
        with g_data.mtx:
            g_data.current_history = g_data.memory.snapshots.copy()
        description = 'Historical live data'

    size = max(0, len(g_data.current_history)-1)
    contents = None
    return 0, size, size, 0, size, [0, size], None, description # [min, max, value], [min, max, [value]], contents, description

@callback([
    Output('export-data-time-start', 'children'), Output('export-data-time-end', 'children')],
    Input('export-time-slider', 'value'), prevent_initial_call=True
)
def show_export_range_time(value):
    """Export slider range."""
    low = min(value[0], value[1])
    high = max(value[0], value[1])
    if low < 0 or high >= len(g_data.current_history):
        return '', ''
    return str(g_data.current_history[low].timestamp), str(g_data.current_history[high].timestamp)

@callback(
        Output('export-data-download', 'data'),
        Input('export-data-btn', 'n_clicks'),
        State('export-time-slider', 'value'),
        prevent_initial_call=True
)
def export_history(n_clicks, value):
    """Exporting historical data."""
    low = min(value[0], value[1])
    high = max(value[0], value[1]) + 1
    if low < 0 or high > len(g_data.current_history):
        return None
    filename = f"export_{datetime.now().strftime('%Y%m%d_%H%M%S')}.pkl"
    return dcc.send_bytes(Snapshot.serialize(g_data.current_history[low:high]), filename)

@callback(Output('in-master-ip', 'value'), Input('submit-master-ip', 'n_clicks'),
          State('in-master-ip', 'value'), prevent_initial_call=True
)
def set_ip(n_clicks: int, value: str):
    """Setting Master IP."""

    global g_data
    g_data.ip = value
    print(f"Updated ip: '{g_data.ip}'")

@callback(Output('dummy', 'hidden', allow_duplicate=True), Input('submit-reset-scanners-dist', 'n_clicks'), prevent_initial_call=True)
def reset_scanners_dist(_):
    global g_data
    with g_data.mtx:
        g_data.memory.reset_scanner_distances()
    return True

@callback(Output('dummy', 'hidden', allow_duplicate=True), Input('submit-reset-scanners-pos', 'n_clicks'), prevent_initial_call=True)
def reset_scanners_pos(_):
    global g_data
    with g_data.mtx:
        g_data.memory.use_2d = False
        g_data.memory.reset_scanner_positions()
    return True

@callback(Output('dummy', 'hidden', allow_duplicate=True), Input('submit-reset-scanners-pos2d', 'n_clicks'), prevent_initial_call=True)
def reset_scanners_pos2d(_):
    global g_data
    with g_data.mtx:
        g_data.memory.use_2d = True
        g_data.memory.reset_scanner_positions()
    return True

@callback(
        Output('known-device-textarea', 'value', allow_duplicate=True), Input('submit-known-device', 'n_clicks'),
        [State('in-known-device-mac', 'value'), State('in-known-device-name', 'value'),
         State('in-known-device-env-factor', 'value'), State('in-known-device-ref-pl', 'value')],
        prevent_initial_call=True
)
def add_known_device(_, mac_str, name_str, ef_str, pl_str):
    global g_data

    if mac_str and Mac.is_valid(mac_str):
        mac = Mac.from_str(mac_str)
        try:
            ef = float(ef_str)
        except:
            ef = DEFAULT_ENV_FACTOR
        try:
            pl = int(pl_str)
        except:
            pl = DEFAULT_REF_PATH_LOSS
        g_data.config_data[mac] = (name_str, ef, pl)
    save_config_data(g_data.data_dir, g_data.config_data)
    return get_config_data_str()

@callback(
        Output('known-device-textarea', 'value', allow_duplicate=True), Input('submit-remove-known-device', 'n_clicks'),
        State('in-known-device-mac', 'value'), prevent_initial_call=True
)
def remove_known_device(_, mac_str):
    global g_data
    if mac_str and Mac.is_valid(mac_str):
        mac = Mac.from_str(mac_str)
        if mac in g_data.config_data:
            del g_data.config_data[mac]
    save_config_data(g_data.data_dir, g_data.config_data)
    return get_config_data_str()

def get_config_data_str():
    out_str = ""
    for k,v in g_data.config_data.items():
        out_str += f'{str(k)} : (Name: {v[0]}), (Environment Factor: {v[1]}), (Path Loss: {v[2]})\n'
    return out_str

def make_detail_table(is_scanner, mac, x, y, z, is_ble = None, is_public_addr = None, event_type = None, adv_data = None):
    """HTML table with device/scanner data."""
    dev_type = 'Scanner' if is_scanner else 'Device'
    common_body = [
        html.Tr([ html.Td('Type'), html.Td(dev_type) ]),
        html.Tr([ html.Td('MAC'), html.Td(mac) ]),
        html.Tr([ html.Td('X'), html.Td(x) ]),
        html.Tr([ html.Td('Y'), html.Td(y) ]),
        html.Tr([ html.Td('Z'), html.Td(z) ])
    ]
    body = []
    if not is_scanner:
        body = [
            html.Tr([ html.Td('BLE'), html.Td(str(is_ble)) ]),
            html.Tr([ html.Td('Public'), html.Td(str(is_public_addr)) ]),
            html.Tr([ html.Td('Event type'), html.Td(str(event_type)) ]),
            html.Tr([ html.Td('Advertising data'), html.Td(str(adv_data)) ])
        ]
    return html.Table([html.Tbody(common_body + body)], style={'width':'auto'})

""" Common figure setup for live and historical data."""


HT2  = "(%{x:.2f}, %{y:.2f})"
HT3  = "(%{x:.2f}, %{y:.2f}, %{z:.2f})"
HT2C = "(%{x:.2f}, %{y:.2f})<br>%{customdata}" # Hovertemplate with custom data 2D
HT3C = "(%{x:.2f}, %{y:.2f}, %{z:.2f})<br>%{customdata}" # Hovertemplate with custom data 3D

def center_scatter_figure(fig2d: go.Figure, fig3d: go.Figure):
    n='Center'
    mode='markers'
    m = dict(size=5, color='rgb(0,0,0)', opacity=0.75)

    fig2d.add_scatter(x=[0.0], y=[0.0], name=n, mode=mode, marker=m, hovertemplate=HT2)
    fig3d.add_scatter3d(x=[0.0], y=[0.0], z=[0.0], name=n, mode=mode, marker=m, hovertemplate=HT3)

def scanners_scatter_figure(fig2d: go.Figure, fig3d: go.Figure,
                            pos: list[list[float], list[float], list[float]],
                            data: list[str]):
    n='Scanner'
    mode='markers'
    m=dict(size=9, color='rgb(255,0,0)', opacity=0.75)

    fig2d.add_scatter(x=pos[0], y=pos[1], name=n, mode=mode, marker=m, customdata=data, hovertemplate=HT2C)
    m['size'] = 6
    fig3d.add_scatter3d(x=pos[0], y=pos[1], z=pos[2], name=n, mode=mode, marker=m, customdata=data, hovertemplate=HT3C)

def devices_line_figure(fig2d: go.Figure, fig3d: go.Figure,
                        pos: list[list[float], list[float], list[float]]):
    n='History'
    m=dict(color='rgb(0,0,255)', opacity=1.0)
    mode='lines'
    fig2d.add_scatter(x=pos[0], y=pos[1], name=n, mode=mode, marker=m, hovertemplate=HT2)
    fig3d.add_scatter3d(x=pos[0], y=pos[1], z=pos[2], name=n, mode=mode, marker=m, hovertemplate=HT3)

def devices_scatter_figure(fig2d: go.Figure, fig3d: go.Figure,
                           pos0: list[list[float], list[float], list[float]], # Known
                           pos1: list[list[float], list[float], list[float]], # Public
                           pos2: list[list[float], list[float], list[float]], # Private
                           data0: list[str], data1: list[str], data2: list[str]):
    n0='Device (Known)'
    n1='Device (Public)'
    n2='Device (Private)'
    m0=dict(size=9, color='rgb(0,255,0)', opacity=0.75)
    m1=dict(size=9, color='rgb(100,0,165)', opacity=0.75)
    m2=dict(size=9, color='rgb(128,120,120)', opacity=0.55)
    mode='markers'

    fig2d.add_scatter(x=pos0[0], y=pos0[1], name=n0, mode=mode, marker=m0, customdata=data0, hovertemplate=HT2C) # Known
    fig2d.add_scatter(x=pos1[0], y=pos1[1], name=n1, mode=mode, marker=m1, customdata=data1, hovertemplate=HT2C) # Public
    fig2d.add_scatter(x=pos2[0], y=pos2[1], name=n2, mode=mode, marker=m2, customdata=data2, hovertemplate=HT2C) # Private

    m0['size'] = 6
    m1['size'] = 6
    m2['size'] = 4
    fig3d.add_scatter3d(x=pos0[0], y=pos0[1], z=pos0[2], name=n0, mode=mode, marker=m0, customdata=data0, hovertemplate=HT3C) # Known
    fig3d.add_scatter3d(x=pos1[0], y=pos1[1], z=pos1[2], name=n1, mode=mode, marker=m1, customdata=data1, hovertemplate=HT3C) # Public
    fig3d.add_scatter3d(x=pos2[0], y=pos2[1], z=pos2[2], name=n2, mode=mode, marker=m2, customdata=data2, hovertemplate=HT3C) # Private

def setup_figure_layout(fig2d: go.Figure, fig3d: go.Figure):
    fig2d.update_layout(showlegend=True, width=1024, height=600, autosize=False)
    fig3d.update_layout(
        showlegend=True, width=600, height=600, autosize=True,
        scene=dict(xaxis=dict(range=[-15.0,15.0]), yaxis=dict(range=[-15.0,15.0]), zaxis=dict(range=[-15.0,15.0])),
        #template='plotly_dark'
    )

    # The ranges refuse to work with update_layout for some reason
    fig2d.update_xaxes(range=[-15.0, 15.0])
    fig2d.update_yaxes(range=[-15.0, 15.0])

    fig2d.update_layout(uirevision='constant2d')
    fig3d.update_layout(uirevision='constant3d')


def load_config_data(dir: str) -> dict[Mac, tuple[float, int]]:
    file = Path(dir + "/data.json")
    dev = {}
    try:
        if file.is_file():
            with file.open() as f:
                data = json.load(f)
                for k,v in data.items():
                    if len(k) != 17: # simple validity check
                        break
                    dev[Mac.from_str(k)] = (v[0], v[1], v[2])
    except Exception as e:
        print(f'Config loading error: {e}')
    return dev

def save_config_data(dir: str, devices: dict[Mac, tuple[str, float, int]]):
    path = Path(dir)
    path.mkdir(parents=True, exist_ok=True)
    file = path/"data.json"
    parsed = {}
    for k,v in devices.items():
        parsed[str(k)] = v
    with file.open('w') as f:
        json.dump(parsed, f)
