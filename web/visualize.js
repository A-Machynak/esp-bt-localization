const textarea = document.getElementById("textarea");
const canvasStatic = document.getElementById("canvas-static");
const canvasEntities = document.getElementById("canvas-entities");
const canvasDynamic = document.getElementById("canvas-dynamic");

const canvasStaticCtx = canvasStatic.getContext("2d");
const canvasEntitiesCtx = canvasEntities.getContext("2d");
const canvasDynamicCtx = canvasDynamic.getContext("2d");
const boardWidth = 800; const boardHeight = 800;
const unitSquare = 25;

var EntityMemoryLimit = 3; // Assumed to be > 1
// Array of dict (Scanners/Devices) { Bda, X, Y, Z, IsScanner, IsBle, IsAddrTypePublic }
let entityMemory = Array.from(Array(EntityMemoryLimit), () => new Array(0));
let entityHeadIndex = 0;
let mouseX = 0, mouseY = 0;

window.addEventListener("load", Init);

function Init() {
	DrawStatic();
	setInterval(Update, 1000);
	setInterval(GetData, 4000);
	canvasDynamic.addEventListener('mousemove', function (event) {
		const p = PositionInCanvas(canvasDynamic, event.clientX, event.clientY);
		mouseX = p[0]; mouseY = p[1];
		RedrawDynamic();
	});
	canvasDynamic.addEventListener('click', function (event) {
		CheckClickOnEntity();
	});

	document.getElementById("set-path-loss").addEventListener("click", function() {
		var bda = CheckAndConvertBda();
		if (bda) {
			ConfigSetPathLoss(bda, document.getElementById("path-loss-value").value);
		}
	});
	document.getElementById("set-env-factor").addEventListener("click", function() {
		var bda = CheckAndConvertBda();
		if (bda) {
			ConfigSetEnvFactor(bda, document.getElementById("env-factor-value").value);
		}
	});
	document.getElementById("set-bda-name").addEventListener("click", function() {
		SetLocalBdaName(document.getElementById("bda").value, document.getElementById("bda-name").value);
		//var bda = CheckAndConvertBda();
		//if (bda) { ConfigSetBdaName(bda, document.getElementById("bda-name").value); }
	});
	document.getElementById("sys-msg-restart").addEventListener("click", function() {
		ConfigSendSystemMsg(0);
	});
	document.getElementById("set-path-limit").addEventListener("click", function() {
		ChangePathLimit(parseInt(document.getElementById("set-path-limit-value").value));
	});
}

function ChangePathLimit(value) {
	if (value < 1) {
		return;
	}
	entityHeadIndex = 0;
	EntityMemoryLimit = value;
	entityMemory = Array.from(Array(EntityMemoryLimit), () => new Array(0));
}

function PositionInCanvas(canvas, x, y) {
	const rect = canvas.getBoundingClientRect();
	return [x - rect.left, y - rect.top];
}

function CheckAndConvertBda() {
	var bda = BdaStringToHex(document.getElementById("bda").value);
	if (typeof bda == "string") {
		alert(bda);
		return false;
	}
	return bda;
}

function ConfigSendSystemMsg(value) {
	if (value != 0) {
		alert("System message can only be of type 0");
		return;
	}
	const buf = new Uint8Array(2);
	buf[0] = 0;      // type
	buf[1] = 0;      // sys msg type
	PostConfig(buf);
}

function ConfigSetPathLoss(bda, value) {
	if (0 > value || value > 100) {
		alert("Path loss should be between 0 and 100");
		return;
	}
	const buf = new Uint8Array(8);
	buf[0] = 1;      // type
	buf.set(bda, 1); // MAC
	buf[7] = value;  // path loss
	PostConfig(buf);
}

function ConfigSetEnvFactor(bda, value) {
	if (0.0 > value || value > 10.0) {
		alert("Environment factor should be between 0 and 10");
		return;
	}
	const buf = new Uint8Array(11);
	buf[0] = 2;      // type
	buf.set(bda, 1); // MAC
	new DataView(buf.buffer).setFloat32(7, value, true);
	PostConfig(buf);
}

function ConfigSetBdaName(bda, value) { // unused
	if (0 >= value.length || value.length >= 16) {
		alert("Name should have between 1 and 16 characters");
		return;
	}
	var buf = new Uint8Array(1 + bda.byteLength + value.length);
	buf[0] = 3; // type
	buf.set(bda, 1); // MAC
	buf.set(new TextEncoder().encode(value), 1 + bda.byteLength); // Name
	PostConfig(buf);
}

function SetLocalBdaName(bda, value) {
	const check = BdaStringToHex(bda);
	if (typeof check == "string") { // only for validation
		alert(check);
		return;
	}
	window.localStorage.setItem(bda, value);
}

function PostConfig(bytes) {
	const xhr = new XMLHttpRequest();
	xhr.open("POST", "/api/config");
	xhr.send(bytes);
}

// Dynamic draw
function Update() {
	UpdateEntities();
	RedrawDynamic();
}

// Dynamic draw - entities updated at certain interval
function UpdateEntities() {
	const ctx = canvasEntitiesCtx;
	ctx.clearRect(0, 0, canvasEntities.width, canvasEntities.height);

	textarea.value = "";

	// { Bda, X, Y, Z, IsScanner, IsBle, IsAddrTypePublic }
	for (var e of entityMemory[entityHeadIndex]) {
		// Append to textarea
		textarea.value += EntityToString(e);

		UpdatePastLines(ctx, e);
		
		const realCoord = ToCanvasCoordinates(e.X, e.Y);
		if (e.IsScanner) {
			DrawScanner(ctx, realCoord[0], realCoord[1], e.Z);
		}
		else {
			DrawDevice(ctx, realCoord[0], realCoord[1], e.Z);
		}
	}
}

function UpdatePastLines(ctx, entity) {
	var prevX, prevY;
	var first = true;
	for (var i = 0; i < entityMemory.length; i++) {
		// Values are inserted as if it's a ring buffer
		const idx = (entityHeadIndex + i + 1) % entityMemory.length;

		const found = entityMemory[idx].find((e) => e.Bda.every((v, i) => v === entity.Bda[i]));
		if (found) {// Draw simple line between old and new position
			const xy = ToCanvasCoordinates(found.X, found.Y);
			if (!first) {
				ctx.beginPath();
				ctx.setLineDash([5,5]);
				ctx.moveTo(prevX, prevY);
				ctx.lineTo(xy[0], xy[1]);
				ctx.stroke();
			}

			prevX = xy[0];
			prevY = xy[1];
			first = false;
		}
	}
}

// Dynamic draw - mouseover. Doesn't need to clear entities.
function RedrawDynamic() {
	const ctx = canvasDynamicCtx;
	const fontSize = 18;
	ctx.clearRect(0, 0, canvasDynamic.width, canvasDynamic.height);
	ctx.font = `${fontSize}px Georgia`;
	
	// { Bda, X, Y, Z, IsScanner, IsBle, IsAddrTypePublic }
	var row = 1;
	const topLeftX = 800 + 5;
	for (var e of entityMemory[entityHeadIndex]) {
		const realCoord = ToCanvasCoordinates(e.X, e.Y);
		const realX = realCoord[0];
		const realY = realCoord[1];
		if (IsInRange(mouseX, mouseY, realX, realY)) {
			const name = window.localStorage.getItem(BdaToString(e.Bda));
			if (name) {
				ctx.fillText(`\"${name}\"`, topLeftX, row++ * (fontSize + 2), 200);
			}
			// Show some detailed information
			ctx.fillText(e.IsScanner ? "Scanner" : "Device", topLeftX, row++ * (fontSize + 2), 200);
			ctx.fillText(BdaToString(e.Bda), topLeftX, row++ * (fontSize + 2), 200);
			ctx.fillText(`${e.IsBle ? "BLE " : "BT "} ${e.IsAddrTypePublic ? "Public " : "Random "} ${e.ScannerCount}`,
				topLeftX, row++ * (fontSize + 2), 200);
			ctx.fillText(`x: ${e.X.toFixed(5)}`, topLeftX, row++ * (fontSize + 2), 200);
			ctx.fillText(`y: ${e.Y.toFixed(5)}`, topLeftX, row++ * (fontSize + 2), 200);
			ctx.fillText(`z: ${e.Z.toFixed(5)}`, topLeftX, row++ * (fontSize + 2), 200);
			row++;
		}
	}
}

function CheckClickOnEntity() {
	for (var e of entityMemory[entityHeadIndex]) {
		const realCoord = ToCanvasCoordinates(e.X, e.Y);
		const realX = realCoord[0];
		const realY = realCoord[1];
		if (IsInRange(mouseX, mouseY, realX, realY)) {
			document.getElementById("bda").value = BdaToString(e.Bda);
			break;
		}
	}
}

// Static draw - single grid. Never changes.
function DrawStatic() {
	DrawGrid(canvasStaticCtx, 0, 0, boardWidth, boardHeight, unitSquare);
}

// Request loop
function GetData() {
	var xhr = new XMLHttpRequest();

	xhr.onreadystatechange = function () {
		if (xhr.readyState == 4 && xhr.status == 200) {
			ParseRawData(xhr.response);
		}
	};
	xhr.open("GET", "/api/devices");
	xhr.responseType = "arraybuffer";
	xhr.send();
}

// Parse raw device data into `entities`. Expects an array of 20B/element
function ParseRawData(rawData) {
	entityHeadIndex++;
	if (entityHeadIndex >= EntityMemoryLimit) {
		entityHeadIndex = 0;
	}
	entityMemory[entityHeadIndex].length = 0;

	// 6B BDA, 3*4B (x,y,z), 1B ScannerCount, 1B Flags
	const singleElement = 20;
	const total = rawData.byteLength / singleElement;

	// Start indices
	const bdaIdx = 0;
	const xyzIdx = 6;
	const sCountIdx = 18;
	const flagsIdx = 19;

	var offset = 0;
	for (var i = 0; i < total; i++) {
		var view = new DataView(rawData, offset, singleElement);

		var bda = [];
		for (var j = bdaIdx; j < bdaIdx+6; j++) {
			bda.push(view.getUint8(j));
		}
		var x = view.getFloat32(xyzIdx, true);
		var y = view.getFloat32(xyzIdx + 4, true);
		var z = view.getFloat32(xyzIdx + 8, true);
		var sCount = view.getUint8(sCountIdx);
		var flags = view.getUint8(flagsIdx);

		entityMemory[entityHeadIndex].push({
			Bda: bda, X: x, Y: y, Z: z,
			ScannerCount: sCount,
			IsScanner: ((flags & 0b00000001) != 0),
			IsBle: ((flags & 0b00000010) != 0),
			IsAddrTypePublic: ((flags & 0b00000100) != 0)
		});
		offset += singleElement;
	}
}

/* Helper functions */
function DrawScanner(ctx, x, y, z) {
	const color = `rgb(255, 165, ${Math.floor(Clamp(z * 12, 0, 128))})`;
	DrawCircle(ctx, x, y, 5, color);

	// Radial gradient around it
	const radius = 100;
	var grad = ctx.createRadialGradient(x, y, 1, x, y, radius);
	grad.addColorStop(0, `rgba(255,165,0,0.25`);
	grad.addColorStop(0.9, `rgba(0,0,0,0.0)`);

	ctx.fillStyle = grad;
	ctx.arc(x, y, radius, 0, 2 * Math.PI);
	ctx.fill();
}

function DrawDevice(ctx, x, y, z) {
	const color = `rgb(0, 165, ${Math.floor(Clamp(z * 12, 0, 128))})`;
	DrawCircle(ctx, x, y, 5, color);
}

function DrawCircle(ctx, x, y, size, color) {
	ctx.beginPath();
	ctx.arc(x, y, size, 0, 2 * Math.PI)
	ctx.fillStyle = color;
	ctx.fill();
}

function DrawGrid(ctx, xStart, yStart, boxWidth, boxHeight, spacing) {
	ctx.beginPath();
	for (var x = xStart; x <= boxWidth; x += spacing) {
		ctx.moveTo(x, yStart);
		ctx.lineTo(x, boxHeight);
	}
	for (var y = yStart; y <= boxHeight; y += spacing) {
		ctx.moveTo(xStart, y);
		ctx.lineTo(boxWidth, y);
	}
	ctx.setLineDash([1, 4]);
	ctx.strokeStyle = "gray";
	ctx.stroke();

	// Single cross in middle
	ctx.beginPath();

	var midX = (boxWidth / 2 + xStart);
	var midY = (boxHeight / 2 + yStart);
	ctx.moveTo(midX - spacing / 2, midY);
	ctx.lineTo(midX + spacing / 2, midY);
	ctx.moveTo(midX, midY - spacing / 2);
	ctx.lineTo(midX, midY + spacing / 2);

	ctx.strokeStyle = "blue";
	ctx.setLineDash([]);
	ctx.stroke();
}

function IsInRange(x1, y1, x2, y2) {
	return (x1 > (x2 - 10)) && ((x2 + 10) > x1)
		&& (y1 > (y2 - 10)) && ((y2 + 10) > y1);
}

function Clamp(v, min, max) {
	return (v <= min) ? min : ((v >= max) ? max : v);
}

function ToCanvasCoordinates(x, y) {
	const xNew = Clamp(x * unitSquare + boardWidth / 2, 0, boardWidth);
	const yNew = Clamp(y * unitSquare + boardHeight / 2, 0, boardHeight);
	return [xNew, yNew];
}
function ByteToHex(byte) {
	const ch = "0123456789ABCDEF";
	return ch[byte >> 4] + ch[byte & 0x0F];
}

function CharToNibble(b) {
	if ('0' <= b && b <= '9') {
		return b - '0';
	}
	else {
		var c = b.charCodeAt(0);
		const a = 'a'.charCodeAt(0);
		const f = 'f'.charCodeAt(0);
		const A = 'A'.charCodeAt(0);
		const F = 'F'.charCodeAt(0);
		if (a <= c && c <= f) {
			return (c - a) + 10;
		}
		else if (A <= c && c <= F) {
			return (c - A) + 10;
		}
	}
	return NaN; // invalid
}

function HexToByte(nib1, nib2) { // 0x[B1,B2]
	const b1 = CharToNibble(nib1);
	const b2 = CharToNibble(nib2);
	if (b1 == NaN || b2 == NaN) {
		return NaN;
	}
	return (b1 << 4) | b2;
}
function BdaToString(bda) {
	return ByteToHex(bda[0]) + ":" + ByteToHex(bda[1]) + ":" + ByteToHex(bda[2]) + ":"
		+ ByteToHex(bda[3]) + ":" + ByteToHex(bda[4]) + ":" + ByteToHex(bda[5]);
}
function BdaStringToHex(str) {
	const errMsg = "Incorrect BDA format. Expected: \"01:23:45:67:89:AB\" (String hex format)";
	if (str.length != 17) {
		return errMsg;
	}

	var array = new Uint8Array(6);
	for (var i = 0; i < 6; i++) {
		const pos = i * 3;
		var b = HexToByte(str[pos], str[pos+1]);
		if (b == NaN) {
			return errMsg;
		}
		array[i] = b;
	}
	return array;
}
function EntityToString(dict) {
	return `${dict.IsScanner ? "Scanner" : "Device"}`
		+ `[${BdaToString(dict.Bda)}]: `
		+ `(${dict.X}, ${dict.Y}, ${dict.Z}), `
		+ `(${dict.IsBle ? "BLE" : "BT"}, ${dict.IsAddrTypePublic ? "Public" : "Random"}, ${dict.ScannerCount})`
		+ "\n";
}
