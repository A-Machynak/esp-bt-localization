const textarea = document.getElementById("textarea");
const canvasStatic = document.getElementById("canvas-static");
const canvasEntities = document.getElementById("canvas-entities");
const canvasDynamic = document.getElementById("canvas-dynamic");

const canvasStaticCtx = canvasStatic.getContext("2d");
const canvasEntitiesCtx = canvasEntities.getContext("2d");
const canvasDynamicCtx = canvasDynamic.getContext("2d");
const boardWidth = 800; const boardHeight = 800;
const unitSquare = 25;

let mouseX = 0, mouseY = 0;
let entities = []; // Array of dict { Bda, X, Y, Z, IsScanner, IsBle, IsAddrTypePublic }

window.addEventListener("load", Init);

function Init() {
	DrawStatic();
	setInterval(Update, 1000);
	setInterval(GetData, 4000);
	canvasDynamic.addEventListener('mousemove', function (event) {
		mouseX = event.clientX; mouseY = event.clientY;
		RedrawDynamic();
	});
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
	for (var e of entities) {
		// Append to textarea
		textarea.value += (e.IsScanner ?
			`Scanner [${BdaToString(e.Bda)}]: (${e.X}, ${e.Y}, ${e.Z})` :
			`Device [${BdaToString(e.Bda)}]: (${e.X}, ${e.Y}, ${e.Z}), ` +
			`(${e.IsBle ? "BLE" : "BT"}, ${e.IsAddrTypePublic ? "Public" : "Random"})`)
			+ "\n";

		const realCoord = ToCanvasCoordinates(e.X, e.Y);
		const realX = realCoord[0];
		const realY = realCoord[1];
		if (e.IsScanner) {
			DrawCircle(ctx, realX, realY, 5,
				`rgb(255, 165, ${Math.floor(Clamp(e.Z * 12, 0, 128))})`);
		}
		else {
			DrawCircle(ctx, realX, realY, 5,
				`rgb(0, 165, ${Math.floor(Clamp(e.Z * 12, 0, 128))})`);
		}
	}
}

// Dynamic draw - mouseover. Doesn't need to clear entities.
function RedrawDynamic() {
	const ctx = canvasDynamicCtx;
	ctx.clearRect(0, 0, canvasDynamic.width, canvasDynamic.height);

	// { Bda, X, Y, Z, IsScanner, IsBle, IsAddrTypePublic }
	for (var e of entities) {
		const realCoord = ToCanvasCoordinates(e.X, e.Y);
		const realX = realCoord[0];
		const realY = realCoord[1];
		if (IsInRange(mouseX, mouseY, realX, realY)) {
			// Show tooltip
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
		else if (xhr.status != 200) {
			console.log("Warning - XHR Status " + xhr.status);
		}
	};
	xhr.open("GET", "/api/devices");
	xhr.responseType = "arraybuffer";
	xhr.send();
}

// Parse raw device data into `entities`. Expects an array of 19B/element
function ParseRawData(rawData) {
	entities.length = 0;

	// 6B BDA, 3*4B (x,y,z), 1B Flags
	const singleElement = 19;
	const total = rawData.byteLength / singleElement;

	const bdaOffset = 6;
	const xyzOffset = bdaOffset + 3 * 4;

	var offset = 0;
	for (var i = 0; i < total; i++) {
		var view = new DataView(rawData, offset, singleElement);

		var bda = [];
		for (var j = 0; j < 6; j++) {
			bda.push(view.getUint8(j));
		}
		var x = view.getFloat32(bdaOffset, true);
		var y = view.getFloat32(bdaOffset + 4, true);
		var z = view.getFloat32(bdaOffset + 8, true);
		var flag = view.getUint8(xyzOffset);

		entities.push({
			Bda: bda, X: x, Y: y, Z: z,
			IsScanner: ((flag & 0b00000001) != 0),
			IsBle: ((flag & 0b00000010) != 0),
			IsAddrTypePublic: ((flag & 0b00000100) != 0)
		});
		offset += singleElement;
	}
}

/* Helper functions */
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
function BdaToString(bda) {
	return ByteToHex(bda[0]) + ":" + ByteToHex(bda[1]) + ":" + ByteToHex(bda[2]) + ":"
		+ ByteToHex(bda[3]) + ":" + ByteToHex(bda[4]) + ":" + ByteToHex(bda[5]);
}