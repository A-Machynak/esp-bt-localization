import io
from datetime import datetime
import base64

import dash
from dash import dcc, html, Input, Output, State, callback
from plotly import graph_objects as go

from common import Mac
from data import Device, Memory, Scanner
from snapshot import Snapshot
from master_comm import get_data, force_advertise

class GlobalData:
    app: dash.Dash
    ip: str
    memory: Memory = Memory()
    current_history: list[Snapshot] = []
g_data = GlobalData()

def start(app_name: str, ip: str):
    global g_data

    PADDING_STYLE = {'padding-left': '1em', 'padding-right': '1em', 'margin': '0'}
    g_data.ip = ip
    g_data.app = dash.Dash(app_name)
    g_data.app.layout = html.Div([
        html.P(id='dummy'),
        html.P('Status: No data', id='status-text'),
        dcc.Tabs(
            id='tabs', value='live-update-tab', children=[
                dcc.Tab(label='Live', id='live-update-tab', value='live-update-tab', children=[
                    dcc.Interval(id='graph-update-interval', interval=3000),
                    html.Hr(),
                    html.Div([
                        html.P("Master IP", style=PADDING_STYLE),
                        dcc.Input(id='in-master-ip', type='text', placeholder='Master IP', value=g_data.ip, style={'width': '25%'}),
                        html.Button('Set', id='submit-master-ip', n_clicks=0),
                    ]),
                    html.Hr(),
                    html.Div([
                        dcc.Graph(id='live-graph3d', figure={}, style={'display': 'inline-block'}),
                        dcc.Graph(id='live-graph2d', figure={}, style={'display': 'inline-block'})
                    ]),
                    html.Hr(),
                    html.Div([], id='detail_table')
                ]),
                dcc.Tab(label='History', id='history-tab', value='history-tab', children=[
                    html.Hr(),
                    html.H1('-', id='history-description-label'),
                    html.H1('-', id='history-time-label'),
                    html.Div([
                        dcc.Graph(id='history-scatter3d', figure={}, style={'display': 'inline-block'}),
                        dcc.Graph(id='history-scatter2d', figure={}, style={'display': 'inline-block'}),
                    ]),
                    dcc.Slider(0, 1, 1, value=1, id='history-time-slider', marks=None, tooltip={"always_visible": True, "template": "{value}"}),
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
                ])
        ]),
        dcc.Interval(id='data-update-interval', interval=3000)
    ], style={'data-bs-theme': "dark"})
    g_data.app.run(debug=True)

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
    # Check for advertising
    scanner = g_data.memory.get_scanner_to_advertise()
    if scanner:
        force_advertise(g_data.ip, scanner)
        print(f"{scanner} should advertise")
    # Save data
    g_data.memory.create_snapshot()
    return True

@callback([Output('live-graph3d', 'figure'), Output('live-graph2d', 'figure')], Input('graph-update-interval', 'n_intervals'))
def update_live_graph(_):
    """Updating 2D/3D scatter live data."""

    fig3d = go.Figure()
    fig2d = go.Figure()
    MARKER_SIZE = 8

    # Center point to use as a reference
    fig3d.add_scatter3d(
        x=[0.0], y=[0.0], z=[0.0], name='Center', mode='markers',
        marker=dict(size=5, color='rgb(0,0,0)', opacity=0.75), hovertemplate="(%{x:.2f}, %{y:.2f}, %{z:.2f})"
    )
    fig2d.add_scatter(
        x=[0.0], y=[0.0], name='Center', mode='markers',
        marker=dict(size=5, color='rgb(0,0,0)', opacity=0.75), hovertemplate="(%{x:.2f}, %{y:.2f})"
    )

    # Scanners
    xs,ys,zs = [], [], []
    data_s = []
    for mac, sc in g_data.memory.scanner_dict.items():
        if sc.has_position():
            xs.append(sc.x)
            ys.append(sc.y)
            zs.append(sc.z)
            data_s.append(mac)
    fig3d.add_scatter3d(
        x=xs, y=ys, z=zs, name='Scanner',
        mode='markers', marker=dict(size=MARKER_SIZE, color='rgb(255,0,0)', opacity=0.75),
        customdata=data_s, hovertemplate="(%{x:.2f}, %{y:.2f}, %{z:.2f})<br>%{customdata}"
    )
    fig2d.add_scatter(
        x=xs, y=ys, name='Scanner',
        mode='markers', marker=dict(size=MARKER_SIZE, color='rgb(255,0,0)', opacity=0.75),
        customdata=data_s, hovertemplate="(%{x:.2f}, %{y:.2f})<br>%{customdata}"
    )

    # Devices
    xd,yd,zd = [1.0], [1.0], [1.0] # Testing
    data_d = [str(Mac([0, 1, 2, 3, 4, 5]))] # Testing
    for mac, dev in g_data.memory.device_dict.items():
        if dev.has_position():
            xd.append(dev.x)
            yd.append(dev.y)
            zd.append(dev.z)
            data_d.append(str(mac))

    fig3d.add_scatter3d(
        x=xd, y=yd, z=zd, name='Device',
        mode='markers', marker=dict(size=MARKER_SIZE, color='rgb(0,255,0)', opacity=0.75),
        customdata=data_d, hovertemplate="(%{x:.2f}, %{y:.2f}, %{z:.2f})<br>%{customdata}"
    )
    fig2d.add_scatter(
        x=xd, y=yd, name='Device',
        mode='markers', marker=dict(size=MARKER_SIZE, color='rgb(0,255,0)', opacity=0.75),
        customdata=data_d, hovertemplate="(%{x:.2f}, %{y:.2f})<br>%{customdata}"
    )

    fig3d.update_layout(
        showlegend=True, autosize=True, width=600, height=600,
        scene=dict(xaxis=dict(range=[-15,15]), yaxis=dict(range=[-15,15]), zaxis=dict(range=[-15,15])),
        #template='plotly_dark'
    )
    fig2d.update_layout(
        showlegend=True, autosize=True, width=1024, height=600,
        scene=dict(xaxis=dict(range=[-15,15]), yaxis=dict(range=[-15,15])),
        #template='plotly_dark'
    )
    fig3d.update_layout(uirevision='constant3d')
    fig2d.update_layout(uirevision='constant2d')
    return fig3d, fig2d

@callback(Output('detail_table', 'children'), [Input('live-graph3d', 'clickData'), Input('live-graph2d', 'clickData')])
def live_graph_click(click_data3d, click_data2d):
    """Showing detailed data for a clicked point."""

    if not click_data3d and not click_data2d:
        return True
    mac_str = ""
    if click_data3d and 'customdata' in click_data3d['points'][0]:
        mac_str = click_data3d['points'][0]['customdata']
    elif click_data2d and 'customdata' in click_data2d['points'][0]:
        mac_str = click_data2d['points'][0]['customdata']
    else:
        return True
    mac = Mac.from_str(mac_str)
    found = g_data.memory.find(mac)
    if not found:
        print(f'{mac} not found?')
        return

    dev_type = 'Scanner' if found is Scanner else 'Device'
    common_body = [
        html.Tr([ html.Td('Type'), html.Td(dev_type, id='detail_label_type') ]),
        html.Tr([ html.Td('MAC'), html.Td(mac_str, id='detail_label_mac') ]),
        html.Tr([ html.Td('X'), html.Td(found.x, id='detail_label_x') ]),
        html.Tr([ html.Td('Y'), html.Td(found.y, id='detail_label_y') ]),
        html.Tr([ html.Td('Z'), html.Td(found.z, id='detail_label_z') ])
    ]
    body = []
    if found is Device:
        body = [
            html.Tr([ html.Td('BLE'), html.Td(found.is_ble, id='detail_label_ble') ]),
            html.Tr([ html.Td('Public'), html.Td(found.is_public_addr, id='detail_label_public') ]),
            html.Tr([ html.Td('Event type'), html.Td(found.event_type.label, id='detail_label_evt_type') ]),
            html.Tr([ html.Td('Advertising data'), html.Td(str(found.advertising_data), id='detail_label_adv_data') ])
        ]
    return html.Table([
        html.Tbody([
            common_body + body
        ])
    ], style={'width':'auto'}) # Table

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
        data = Snapshot.load(decoded)
        if data is not None:
            g_data.current_history = data
            description = filename
    elif tab_name == 'history-tab':
        # Copy snapshots; so they don't get updated while the user is playing with it
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
    filename = f"export_{datetime.timestamp(datetime.now())}.pkl"
    return dcc.send_bytes(Snapshot.serialize(g_data.current_history[low:high]), filename)

@callback([
    Output('history-scatter3d', 'figure'),
    Output('history-scatter2d', 'figure'),
    Output('history-time-label', 'children')],
    Input('history-time-slider', 'value'))
def update_history_graph(value):
    """Updating 2D/3D scatter for historical data."""

    fig3d = go.Figure()
    fig2d = go.Figure()

    MARKER_SIZE=8

    # Center point to use as a reference
    fig3d.add_scatter3d(
        x=[0.0], y=[0.0], z=[0.0], name='Center', mode='markers',
        marker=dict(size=5, color='rgb(0,0,0)', opacity=0.75), hovertemplate="(%{x:.2f}, %{y:.2f}, %{z:.2f})"
    )
    fig2d.add_scatter(
        x=[0.0], y=[0.0], name='Center', mode='markers',
        marker=dict(size=5, color='rgb(0,0,0)', opacity=0.75), hovertemplate="(%{x:.2f}, %{y:.2f})"
    )

    if len(g_data.current_history) <= 0 or value >= len(g_data.current_history):
        return fig3d, fig2d, ''
    data = g_data.current_history[value]

     # Scanners
    xs,ys,zs = [], [], []
    data_s = []
    for sc in data.scanners:
        xs.append(sc.x)
        ys.append(sc.y)
        zs.append(sc.z)
        data_s.append(str(sc.mac))
    fig3d.add_scatter3d(
        x=xs, y=ys, z=zs, name='Scanner', mode='markers',
        marker=dict(size=MARKER_SIZE, color='rgb(255,0,0)', opacity=0.75),
        customdata=data_s, hovertemplate="(%{x:.2f}, %{y:.2f}, %{z:.2f})<br>%{customdata}"
    )
    fig2d.add_scatter(
        x=xs, y=ys, name='Scanner', mode='markers',
        marker=dict(size=MARKER_SIZE, color='rgb(255,0,0)',opacity=0.75),
        customdata=data_s, hovertemplate="(%{x:.2f}, %{y:.2f})<br>%{customdata}"
    )

    # Devices
    xd,yd,zd = [], [], []
    data_d = []
    for dev in data.devices:
        xd.append(dev.x)
        yd.append(dev.y)
        zd.append(dev.z)
        data_d.append(str(dev.mac))

    fig3d.add_scatter3d(
        x=xd, y=yd, z=zd, name='Device',
        mode='markers', marker=dict(size=MARKER_SIZE, color='rgb(0,255,0)', opacity=0.75),
        customdata=data_d, hovertemplate="(%{x:.2f}, %{y:.2f}, %{z:.2f})<br>%{customdata}"
    )
    fig2d.add_scatter(
        x=xd, y=yd, name='Device',
        mode='markers', marker=dict(size=MARKER_SIZE, color='rgb(0,255,0)', opacity=0.75),
        customdata=data_d, hovertemplate="(%{x:.2f}, %{y:.2f})<br>%{customdata}"
    )

    fig3d.update_layout(
        showlegend=True, autosize=True, width=600, height=600,
        scene=dict(xaxis=dict(range=[-15,15]), yaxis=dict(range=[-15,15]), zaxis=dict(range=[-15,15])),
        #template='plotly_dark'
    )
    fig2d.update_layout(
        showlegend=True, autosize=True, width=1024, height=600,
        scene=dict(xaxis=dict(range=[-15,15]), yaxis=dict(range=[-15,15])),
        #template='plotly_dark'
    )
    fig3d.update_layout(uirevision='constant3d')
    fig2d.update_layout(uirevision='constant2d')
    return fig3d, fig2d, data.timestamp.strftime("%Y/%m/%d, %H:%M:%S")

@callback(Output('status-text', 'children'), [Input('data-update-interval', 'n_intervals')])
def get_master_data(_):
    """Pulling data from endpoint."""

    #if update_data():
    #    return "Ok"
    return "No data"

@callback(Output('in-master-ip', 'value'), Input('submit-master-ip', 'n_clicks'),
          State('in-master-ip', 'value'), prevent_initial_call=True
)
def set_ip(n_clicks: int, value: str):
    """Setting Master IP."""

    global g_data
    g_data.ip = value
    print(f"Updated ip: '{g_data.ip}'")
