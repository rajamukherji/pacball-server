console.log("Hello world!")

let socket = null;
let pitch = document.getElementById("pitch");
let score1 = document.getElementById("score1");
let score2 = document.getElementById("score2");
let gametime = document.getElementById("gametime");
let ready = document.getElementById("ready");

let createButton = document.getElementById("create-button");
let joinButton = document.getElementById("join-button");
let startButton = document.getElementById("start-button");
let endButton = document.getElementById("end-button");
let leaveButton = document.getElementById("leave-button");

let joinDialog = document.getElementById("join-dialog");
let joinTable = document.getElementById("join-table");

let events = {};
let ping, lag = 0;
let players = [];
let ball = null;
let x = 0, y = 0;
let cx = pitch.offsetLeft + 540;
let cy = pitch.offsetTop + 300;
let interval = null;
let score = null;
let base = null;
let delta = null;

function connect(callback) {
	socket = new WebSocket(((window.location.protocol === "https:") ? "wss://" : "ws://") + window.location.host + "/connect?id=" + id + "&name=" + encodeURIComponent(name));
	socket.onmessage = function(message) {
		let decoded = JSON.parse(message.data);
		for (type in decoded) {
			let data = decoded[type];
			//console.log(type, data);
			events[type](data);
		}
	}
	socket.onopen = function() {
		ping = Date.now();
		socket.send(JSON.stringify({ping: null}));
		if (callback) callback();
	}
	return socket;
}

let id = sessionStorage.getItem("id");
if (id === null) {
	let arr = new Uint8Array(16);
	window.crypto.getRandomValues(arr);
	id = Array.from(arr).map(x => x.toString(16).padStart(2, "0")).join("");
	sessionStorage.setItem("id", id);
}
let name = sessionStorage.getItem("name");
if (name === null) {
	name = prompt("Player Name");
	sessionStorage.setItem("name", name);
}

connect();

function send(event, data) {
	if (socket.readyState != 1) {
		connect(send.bind(null, ...arguments));
	} else {
		let message = {};
		message[event] = data;
		socket.send(JSON.stringify(message));
	}
}

window.send = send;

startButton.style.display = "none";
endButton.style.display = "none";
leaveButton.style.display = "none";

createButton.onclick = function() {
	createButton.style.display = "none";
	joinButton.style.display = "none";
	send("game/create", {name: prompt("Game Name")});
};

startButton.onclick = function() {
	startButton.style.display = "none";
	endButton.style.display = null;
	send("game/start", {});
};

joinButton.onclick = function() {
	joinDialog.showModal();
	send("game/list", {});
};

leaveButton.onclick = function() {
	createButton.style.display = null
	joinButton.style.display = null;
	leaveButton.style.display = "none";
	send("game/leave", {});
};

endButton.onclick = function() {
	send("game/end", {});
};

events["pong"] = function(data) {
	lag = (Date.now() - ping) / 2000;
	//console.log("delay", lag);
};

window.addEventListener("resize", function(event) {
	cx = pitch.offsetLeft + 540;
	cy = pitch.offsetTop + 300;
});

document.body.addEventListener("mousemove", function(event) {
	x = (event.pageX - cx) / 6;
	y = (event.pageY - cy) / 6;
});

document.body.addEventListener("mousedown", function(event) {
	console.log(event);
	let time = Date.now() / 1000 - delta;
	send("game/event", [time - lag, x, y, "KickMedium"]);
});

window.addEventListener("keydown", function(event) {
	console.log(event);
	let time = Date.now() / 1000 - delta;
	switch (event.code) {
	case "Digit1": send("game/event", [time - lag, x, y, "KickShort"]); break;
	case "Digit2": send("game/event", [time - lag, x, y, "KickMedium"]); break;
	case "Digit3": send("game/event", [time - lag, x, y, "KickLong"]); break;
	}
});

logs = document.getElementById("logs");

events["connect"] = function(data) {
};

events["game/create"] = function(data) {
	startButton.style.display = null;
	logs.appendChild(document.createTextNode("Game created\n"));
};

events["game/list"] = function(data) {
	let child;
	while ((child = joinTable.firstChild)) joinTable.removeChild(child);
	data.forEach(game => {
		joinTable.appendChild(create("tr",
			create("td", game.name),
			create("td", game.count.toString() + " players"),
			create("td", create("button", "Join", {"on-click": function() {
				joinDialog.close();
				send("game/join", {game: game.id, password: ""});
			}}))
		));
	});
	if (joinDialog.open) setTimeout(() => send("game/list", {}), 1000);
};

events["game/join"] = function(data) {
	logs.appendChild(document.createTextNode(`Player ${data[0]} joined.\n`));
	if (data[1]) {
		createButton.style.display = "none";
		joinButton.style.display = "none";
		leaveButton.style.display = null;
	}
};

function tick() {
	let time = Date.now() / 1000 - delta;
	let seconds = Math.floor(time);
	gametime.textContent = Math.floor(seconds / 60).toString() + ":" + (seconds % 60).toString().padStart(2, "0");
	send("game/event", [time, x, y, "Move"]);
	let dt = time - base;
	players.forEach(player => {
		let style = player.element.style;
		style.left = (player.x + dt * player.dx + 90) * 6 - 15 + "px";
		style.top = (player.y + dt * player.dy + 50) * 6 - 15 + "px";
		player.element.classList.remove("handler");
	});
	let style = ball.element.style;
	let handler = ball.handler;
	if (handler == -1) {
		style.display = null;
		ready.style.display = "none";
		style.left = (ball.x + dt * ball.dx + 90) * 6 - 6 + "px";
		style.top = (ball.y + dt * ball.dy + 50) * 6 - 6 + "px";
	} else {
		style.display = "none";
		if (handler >= 0) {
			players[handler].element.classList.add("handler");
		} else {
			ready.style.display = null;
		}
	}
}

events["game/start"] = function(data) {
	logs.appendChild(document.createTextNode("Game started\n"));
	for (let element = pitch.firstChild; element; element = pitch.firstChild) {
		pitch.removeChild(element);
	}
	pitch.appendChild(ready);
	let index = data[0];
	players = data[1].map((info, i) => {
		let name = info[0];
		let team = info[1];
		let element = document.createElement("div");
		element.classList.add("player");
		element.classList.add("team" + team.toString());
		let label = document.createElement("span");
		if (i == index) {
			label.textContent = "☺";
		} else {
			let names = name.split(" ");
			let initials = names[0][0].toUpperCase() + names[names.length - 1][0].toUpperCase();
			label.textContent = initials;
		}
		element.appendChild(label);
		pitch.appendChild(element);
		return {name, team, element, x: 0, y: 0, dx: 0, dy: 0};
	});
	let element = document.createElement("div");
	element.classList.add("ball");
	ball = {element, x: 0, y: 0, dx: 0, dy: 0, handler: -2};
	pitch.appendChild(element);
	delta = Date.now() / 1000 - data[0];
	interval = setInterval(tick, 50);
};

events["game/leave"] = function(data) {
	logs.appendChild(document.createTextNode(`Player ${data[0]} left.\n`));
	if (interval != null) clearInterval(interval);
	interval = null;
};

events["game/state"] = function(data) {
	createButton.style.display = "none";
	joinButton.style.display = "none";
	leaveButton.style.display = null;
	base = data[0];
	delta = Date.now() / 1000 - base;
	score = data[1];
	score1.textContent = score[0].toString();
	score2.textContent = score[1].toString();
	let ballData = data[2];
	if (ballData === null) {
		ball.handler = -2;
	} else if (typeof(ballData) === "number") {
		ball.handler = ballData - 1;
	} else {
		ball.handler = -1;
		ball.x = ballData[0];
		ball.y = ballData[1];
		ball.dx = ballData[2];
		ball.dy = ballData[3];
	}
	players.forEach((player, i) => {
		let playerData = data[i + 3];
		player.x = playerData[0];
		player.y = playerData[1];
		player.dx = playerData[2];
		player.dy = playerData[3];
	});
};

events["game/end"] = function(data) {
	startButton.style.display = "none";
	endButton.style.display = "none";
	leaveButton.style.display = "none";
	createButton.style.display = null
	joinButton.style.display = null;
	if (interval != null) clearInterval(interval);
	interval = null;
};
