#include <minilang/ml_library.h>
#include <minilang/ml_macros.h>
#include <minilang/ml_object.h>
#include <chipmunk/chipmunk.h>
#include <string.h>

typedef struct event_t event_t;
typedef struct player_t player_t;
typedef struct game_t game_t;

typedef enum {ACTION_NONE, ACTION_SHORT_KICK, ACTION_MEDIUM_KICK, ACTION_LONG_KICK} action_t;

struct event_t {
	event_t *Next;
	player_t *Player;
	cpVect Target;
	cpFloat Time;
	action_t Action;
};

typedef struct {
	cpVect Position, Velocity, Target;
	cpFloat Angle, AngularVelocity;
} player_state_t;

typedef struct {
	struct {
		player_t *Handler;
		cpVect Position, Velocity;
		cpFloat Angle, AngularVelocity;
	} Ball;
	cpFloat Time;
	player_state_t Players[];
} game_state_t;

struct game_t {
	ml_type_t *Type;
	player_t *Players;
	event_t *Events;
	cpSpace *Space;
	cpBody *Ball;
	game_state_t *Base, *State;
	int NumPlayers, StateSize;
};

struct player_t {
	ml_type_t *Type;
	player_t *Next;
	game_t *Game;
	cpBody *Body, *Target;
	cpConstraint *Joint;
	int Index, Team;
};

const cpCollisionType COLLISION_BALL = 0;
const cpCollisionType COLLISION_PLAYER = 1;
const cpCollisionType COLLISION_WALL = 2;
const cpCollisionType COLLISION_GOAL = 3;
const cpCollisionType COLLISION_FEET = 4;

#define TIME_STEP 0.01

#define BALL_SIZE 2
#define BALL_MASS 1

#define SHORT_KICK_SPEED 2
#define MEDIUM_KICK_SPEED 3
#define LONG_KICK_SPEED 4

#define PLAYER_SIZE 5
#define PLAYER_FEET_SIZE 2
#define PLAYER_MASS 10

#define DRIBBLE_BIAS 100
#define DRIBBLE_FORCE 10000
#define RUN_BIAS 200
#define RUN_FORCE 10000

#define PITCH_WIDTH 180
#define PITCH_HEIGHT 100
#define PITCH_FRICTION 100

static cpBool collide_feet_ball(cpArbiter *Arb, cpSpace *Space, game_t *Game) {
	// TODO: If ball touches player's "mouth", then give the ball to the player and return false;
	CP_ARBITER_GET_BODIES(Arb, BodyA, BodyB);
	player_t *Player = cpBodyGetUserData(BodyA);
	printf("Player %d gets ball!\n", Player->Index);
	return cpFalse;
}

static void collide_player_player(cpArbiter *Arb, cpSpace *Space, game_t *Game) {
	CP_ARBITER_GET_BODIES(Arb, BodyA, BodyB);
	player_t *PlayerA = cpBodyGetUserData(BodyA);
	player_t *PlayerB = cpBodyGetUserData(BodyB);
	game_state_t *State = Game->State;
	if (State->Ball.Handler == PlayerA) {
		cpVect Impulse = cpArbiterTotalImpulse(Arb);
		// TODO: If Impulse is high enough, free the ball.
	} else if (State->Ball.Handler == PlayerB) {
		cpVect Impulse = cpArbiterTotalImpulse(Arb);
		// TODO: If Impulse is high enough, free the ball.
	}
}

game_t *game() {
	game_t *Game = new(game_t);
	cpSpace *Space = Game->Space = cpSpaceNew();
	cpSpaceSetUserData(Space, Game);

	cpBody *Ball = Game->Ball = cpBodyNew(BALL_MASS, cpMomentForCircle(BALL_MASS, 0, BALL_SIZE, cpvzero));
	cpBodySetUserData(Ball, Game);
	cpShape *Shape = cpCircleShapeNew(Ball, BALL_SIZE, cpvzero);
	cpShapeSetElasticity(Shape, 1);
	cpShapeSetCollisionType(Shape, COLLISION_BALL);
	cpSpaceAddShape(Space, Shape);
	cpSpaceAddBody(Game->Space, Ball);

	cpBody *Static = cpSpaceGetStaticBody(Space);
	cpFloat W = PITCH_WIDTH / 2, H = PITCH_HEIGHT / 2;
	Shape = cpSegmentShapeNew(Static, cpv(-W, -H), cpv(-W, H), 0);
	cpShapeSetElasticity(Shape, 1);
	cpShapeSetCollisionType(Shape, COLLISION_WALL);
	cpSpaceAddShape(Space, Shape);
	Shape = cpSegmentShapeNew(Static, cpv(W, -H), cpv(W, H), 0);
	cpShapeSetElasticity(Shape, 1);
	cpShapeSetCollisionType(Shape, COLLISION_WALL);
	cpSpaceAddShape(Space, Shape);
	Shape = cpSegmentShapeNew(Static, cpv(-W, -H), cpv(W, -H), 0);
	cpShapeSetElasticity(Shape, 1);
	cpShapeSetCollisionType(Shape, COLLISION_WALL);
	cpSpaceAddShape(Space, Shape);
	Shape = cpSegmentShapeNew(Static, cpv(-W, H), cpv(W, H), 0);
	cpShapeSetElasticity(Shape, 1);
	cpShapeSetCollisionType(Shape, COLLISION_WALL);
	cpSpaceAddShape(Space, Shape);
	cpConstraint *Friction = cpPivotJointNew2(Static, Ball, cpvzero, cpvzero);
	cpConstraintSetMaxForce(Friction, PITCH_FRICTION);
	cpConstraintSetMaxBias(Friction, 0);
	cpSpaceAddConstraint(Space, Friction);


	cpCollisionHandler *Handler;
	Handler = cpSpaceAddCollisionHandler(Space, COLLISION_FEET, COLLISION_BALL);
	Handler->preSolveFunc = (cpCollisionPreSolveFunc)collide_feet_ball;
	Handler->userData = Game;
	Handler = cpSpaceAddCollisionHandler(Space, COLLISION_PLAYER, COLLISION_PLAYER);
	Handler->postSolveFunc = (cpCollisionPostSolveFunc)collide_player_player;
	Handler->userData = Game;
	return Game;
}

player_t *game_player(game_t *Game, int Team) {
	player_t *Player = new(player_t);
	Player->Game = Game;
	Player->Team = Team;
	Player->Next = Game->Players;
	Game->Players = Player;
	cpVect Position = cpv(
		0.9 * PITCH_WIDTH * ((double)rand() / RAND_MAX - 0.5),
		0.9 * PITCH_HEIGHT * ((double)rand() / RAND_MAX - 0.5)
	);
	cpBody *Body = Player->Body = cpBodyNew(PLAYER_MASS, PLAYER_MASS);
	cpBodySetUserData(Body, Player);
	cpShape *Shape = cpCircleShapeNew(Body, PLAYER_SIZE, cpv(0, 0));
	cpShapeSetCollisionType(Shape, COLLISION_PLAYER);
	cpSpaceAddShape(Game->Space, Shape);
	Shape = cpCircleShapeNew(Body, PLAYER_FEET_SIZE, cpv(PLAYER_SIZE, 0));
	cpShapeSetCollisionType(Shape, COLLISION_FEET);
	cpSpaceAddShape(Game->Space, Shape);
	cpBodySetPosition(Body, Position);
	cpSpaceAddBody(Game->Space, Body);
	cpBody *Target = Player->Target = cpBodyNewStatic();
	cpBodySetPosition(Target, Position);
	cpConstraint *Joint = Player->Joint = cpPivotJointNew2(Target, Body, cpvzero, cpv(2 * PLAYER_SIZE, 0));
	cpConstraintSetMaxBias(Joint, RUN_BIAS);
	cpConstraintSetMaxForce(Joint, RUN_FORCE);
	cpSpaceAddConstraint(Game->Space, Joint);
	cpBody *Static = cpSpaceGetStaticBody(Game->Space);
	cpConstraint *Friction = cpPivotJointNew2(Static, Body, cpvzero, cpvzero);
	cpConstraintSetMaxForce(Friction, PITCH_FRICTION);
	cpConstraintSetMaxBias(Friction, 0);
	cpSpaceAddConstraint(Game->Space, Friction);
	return Player;
}

void game_start(game_t *Game) {
	srand(time(0));
	int NumPlayers = 0;
	for (player_t *Player = Game->Players; Player; Player = Player->Next) Player->Index = NumPlayers++;
	Game->NumPlayers = NumPlayers;
	int StateSize = Game->StateSize = sizeof(game_state_t) + NumPlayers * sizeof(player_state_t);
	game_state_t *State = Game->Base = (game_state_t *)snew(StateSize);
	player_state_t *PlayerState = State->Players;
	for (player_t *Player = Game->Players; Player; Player = Player->Next, ++PlayerState) {
		cpBody *Body = Player->Body;
		PlayerState->Position = PlayerState->Target = cpBodyGetPosition(Body);
		PlayerState->Velocity = cpBodyGetVelocity(Body);
		PlayerState->Angle = cpBodyGetAngle(Player->Body);
		PlayerState->AngularVelocity = cpBodyGetAngularVelocity(Body);
	}
	State->Ball.Position = State->Ball.Velocity = cpvzero;
	State->Ball.Handler = NULL;
	State->Time = 0;
	Game->State = (game_state_t *)snew(StateSize);
}

static event_t *EventCache = NULL;

void player_event(player_t *Player, cpFloat Time, cpVect Target, action_t Action) {
	game_t *Game = Player->Game;
	// Add a player action (movement and/or kick) to game events.
	// If time is older than last base game state, action is discarded.
	if (Time <= Game->Base->Time) return;
	//if (Time > Game->MaxTime) return;
	event_t *Event = EventCache;
	if (!Event) {
		Event = new(event_t);
	} else {
		EventCache = Event->Next;
	}
	Event->Player = Player;
	Event->Time = Time;
	Event->Action = Action;
	Event->Target = Target;
	event_t **Slot = &Game->Events;
	while (Slot[0] && Slot[0]->Time < Time) Slot = &Slot[0]->Next;
	Event->Next = Slot[0];
	Slot[0] = Event;
}

void game_predict(game_t *Game, cpFloat Target) {
	// Predicts the game state at some time after the current base state, applying known events as necessary.
	game_state_t *State = Game->State;
	memcpy(State, Game->Base, Game->StateSize);
	cpBody *Ball = Game->Ball;
	cpBodySetPosition(Ball, State->Ball.Position);
	cpBodySetVelocity(Ball, State->Ball.Velocity);
	player_state_t *PlayerState = State->Players;
	for (player_t *Player = Game->Players; Player; Player = Player->Next, ++PlayerState) {
		cpBody *Body = Player->Body;
		cpBodySetPosition(Body, PlayerState->Position);
		cpBodySetVelocity(Body, PlayerState->Velocity);
		cpBodySetAngle(Body, PlayerState->Angle);
		cpBodySetAngularVelocity(Body, PlayerState->AngularVelocity);
		cpBodySetPosition(Player->Target, PlayerState->Target);
		cpConstraint *Joint = Player->Joint;
		if (State->Ball.Handler == Player) {
			cpConstraintSetMaxBias(Joint, DRIBBLE_BIAS);
			cpConstraintSetMaxForce(Joint, DRIBBLE_FORCE);
		} else {
			cpConstraintSetMaxBias(Joint, RUN_BIAS);
			cpConstraintSetMaxForce(Joint, RUN_FORCE);
		}
	}
	cpSpace *Space = Game->Space;
	cpFloat Time = State->Time;
	for (event_t *Event = Game->Events; Event && Event->Time < Target; Event = Event->Next) {
		while (Time < Event->Time) {
			cpSpaceStep(Space, TIME_STEP);
			Time += TIME_STEP;
		}
		//cpSpaceStep(Space, Event->Time - Time);
		// TODO: Check for goals and ball acquisition
		Time = Event->Time;
		player_t *Player = Event->Player;
		cpBodySetPosition(Player->Target, Event->Target);
		cpConstraint *Joint = Player->Joint;
		if (State->Ball.Handler == Player) {
			if (Event->Action == ACTION_NONE) {
				cpConstraintSetMaxBias(Joint, DRIBBLE_BIAS);
				cpConstraintSetMaxForce(Joint, DRIBBLE_FORCE);
			} else {
				static cpFloat Impulses[] = {
					[ACTION_NONE] = 0,
					[ACTION_SHORT_KICK] = SHORT_KICK_SPEED,
					[ACTION_MEDIUM_KICK] = MEDIUM_KICK_SPEED,
					[ACTION_LONG_KICK] = LONG_KICK_SPEED
				};
				State->Ball.Handler = NULL;
				cpVect Direction = cpvnormalize(cpBodyGetVelocity(Player->Body));
				cpBodyApplyImpulseAtLocalPoint(Ball, cpvmult(Direction, Impulses[Event->Action]), cpvzero);
				cpConstraintSetMaxBias(Joint, RUN_BIAS);
				cpConstraintSetMaxForce(Joint, RUN_FORCE);
			}
		} else {
			cpConstraintSetMaxBias(Joint, RUN_BIAS);
			cpConstraintSetMaxForce(Joint, RUN_FORCE);
		}
	}
	while (Time < Target) {
		cpSpaceStep(Space, TIME_STEP);
		Time += TIME_STEP;
	}
	State->Time = Time;
	//cpSpaceStep(Space, Target - Time);
	State->Ball.Position = cpBodyGetPosition(Game->Ball);
	State->Ball.Velocity = cpBodyGetVelocity(Game->Ball);
	PlayerState = State->Players;
	for (player_t *Player = Game->Players; Player; Player = Player->Next, ++PlayerState) {
		cpBody *Body = Player->Body;
		PlayerState->Position = PlayerState->Target = cpBodyGetPosition(Body);
		PlayerState->Velocity = cpBodyGetVelocity(Body);
		PlayerState->Angle = cpBodyGetAngle(Player->Body);
		PlayerState->AngularVelocity = cpBodyGetAngularVelocity(Body);
		PlayerState->Target = cpBodyGetPosition(Player->Target);
	}
}

void game_rebase(game_t *Game, cpFloat Target) {
	// Updates game base state to specified time (using game_predict).
	game_predict(Game, Target);
	game_state_t *State = Game->State;
	Game->State = Game->Base;
	Game->Base = State;
	event_t *Event = Game->Events;
	if (Event && Event->Time < Target) {
		event_t **Slot = &Event->Next;
		while (Slot[0] && Slot[0]->Time < Target) Slot = &Slot[0]->Next;
		Game->Events = Slot[0];
		Slot[0] = EventCache;
		EventCache = Event;
	}
}

ML_TYPE(GameT, (), "game");
ML_TYPE(PlayerT, (), "player");
ML_ENUM2(ActionT, "action",
	"None", ACTION_NONE,
	"ShortKick", ACTION_SHORT_KICK,
	"MediumKick", ACTION_MEDIUM_KICK,
	"LongKick", ACTION_LONG_KICK
);

ML_METHOD(GameT) {
	game_t *Game = game();
	Game->Type = GameT;
	return (ml_value_t *)Game;
}

ML_METHOD("size", GameT) {
	return ml_tuplev(2, ml_real(PITCH_WIDTH), ml_real(PITCH_HEIGHT));
}

ML_METHOD("start", GameT) {
	game_t *Game = (game_t *)Args[0];
	game_start(Game);
	return (ml_value_t *)Game;
}

ML_METHOD("player", GameT, MLIntegerT) {
	game_t *Game = (game_t *)Args[0];
	player_t *Player = game_player(Game, ml_integer_value(Args[1]));
	Player->Type = PlayerT;
	return (ml_value_t *)Player;
}

ML_METHOD("event", PlayerT, MLRealT, MLRealT, MLRealT, ActionT) {
	player_t *Player = (player_t *)Args[0];
	player_event(Player, ml_real_value(Args[1]), cpv(ml_real_value(Args[2]), ml_real_value(Args[3])), ml_enum_value_value(Args[4]));
	return (ml_value_t *)Player;
}

ML_METHOD("predict", GameT, MLRealT) {
	game_t *Game = (game_t *)Args[0];
	if (!Game->State) return ml_error("StateError", "Game not started yet");
	game_predict(Game, ml_real_value(Args[1]));
	game_state_t *State = Game->State;
	ml_value_t *Result = ml_list();
	ml_list_put(Result, ml_tuplev(5,
		ml_real(State->Ball.Position.x), ml_real(State->Ball.Position.y),
		ml_real(State->Ball.Velocity.x), ml_real(State->Ball.Velocity.y),
		State->Ball.Handler ? ml_integer(State->Ball.Handler->Index) : MLNil
	));
	player_state_t *PlayerState = State->Players;
	for (player_t *Player = Game->Players; Player; Player = Player->Next, ++PlayerState) {
		ml_list_put(Result, ml_tuplev(6,
			ml_real(PlayerState->Position.x), ml_real(PlayerState->Position.y),
			ml_real(PlayerState->Velocity.x), ml_real(PlayerState->Velocity.y),
			ml_real(PlayerState->Target.x), ml_real(PlayerState->Target.y)
		));
	}
	return Result;
}

ML_METHOD("rebase", GameT, MLRealT) {
	game_t *Game = (game_t *)Args[0];
	game_rebase(Game, ml_real_value(Args[1]));
	return (ml_value_t *)Game;
}

ML_LIBRARY_ENTRY0(game) {
#include "game_init.c"
	stringmap_insert(GameT->Exports, "player", PlayerT);
	stringmap_insert(GameT->Exports, "action", ActionT);
	Slot[0] = (ml_value_t *)GameT;
}
