
/*
 *
 * bonus.cpp
 *
 * 23rd August 2005: Created bonus.c
 * 3rd February 2009: Renamed bonus.c to bonus.cpp
 *
 * Part of the OpenJazz project
 *
 *
 * Copyright (c) 2005-2010 Alister Thomson
 *
 * OpenJazz is distributed under the terms of
 * the GNU General Public License, version 2.0
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*
 * Deals with the loading, running and freeing of bonus levels.
 *
 */


#include "bonus.h"
#include "game/game.h"
#include "game/gamemode.h"
#include "io/controls.h"
#include "io/file.h"
#include "io/gfx/font.h"
#include "io/gfx/paletteeffects.h"
#include "io/gfx/sprite.h"
#include "io/gfx/video.h"
#include "io/sound.h"
#include "player/bonusplayer.h"
#include "util.h"

#include <string.h>


int Bonus::loadSprites () {

	File *file;
	unsigned char* pixels;
	int pos, maskLength, pixelsLength;
	int width, height;
	int count;

	try {

		file = new File(F_BONUS, false);

	} catch (int e) {

		return e;

	}

	file->seek(2, true);

	sprites = file->loadShort();
	spriteSet = new Sprite[sprites];

	for (count = 0; count < sprites; count++) {

		spriteSet[count].xOffset = 0;
		spriteSet[count].yOffset = 0;

		// Load dimensions
		width = file->loadShort();
		height = file->loadShort();

		pixelsLength = file->loadShort();
		maskLength = file->loadShort();

		// Sprites can be either masked or not masked.
		if (pixelsLength != 0xFFFF) {

			// Masked
			width <<= 2;

			pos = file->tell() + (pixelsLength << 2) + maskLength;

			// Read scrambled, masked pixel data
			pixels = file->loadPixels(width * height, 0);
			spriteSet[count].setPixels(pixels, width, height, 0);

			delete[] pixels;

			file->seek(pos, true);

		} else if (width) {

			// Not masked

			// Read pixel data
			pixels = file->loadBlock(width * height);
			spriteSet[count].setPixels(pixels, width, height, 0);

			delete[] pixels;

		} else {

			// Zero-length sprite

			// Create blank sprite
			spriteSet[count].clearPixels();

		}

	}

	delete file;

	return E_NONE;

}


int Bonus::loadTiles (char *fileName) {

	File *file;
	unsigned char *pixels;
	unsigned char *sorted;
	int count, x, y;

	try {

		file = new File(fileName, false);

	} catch (int e) {

		return e;

	}

	// Load background
	pixels = file->loadRLE(832 * 20);
	sorted = new unsigned char[512 * 20];

	for (count = 0; count < 20; count++) memcpy(sorted + (count * 512), pixels + (count * 832), 512);

	background = createSurface(sorted, 512, 20);

	delete[] sorted;
	delete[] pixels;

	// Load palette
	file->loadPalette(palette);

	// Load tile graphics
	pixels = file->loadRLE(1024 * 60);
	tileSet = createSurface(pixels, 32, 32 * 60);

	// Create mask
	for (count = 0; count < 60; count++) {

		memset(mask[count], 0, 64);

		for (y = 0; y < 32; y++) {

			for (x = 0; x < 32; x++) {

				if ((pixels[(count << 10) + (y << 5) + x] & 240) == 192)
					mask[count][((y << 1) & 56) + ((x >> 2) & 7)] = 1;

			}

		}

	}

	delete[] pixels;

	delete file;

	return E_NONE;

}


Bonus::Bonus (char * fileName, unsigned char diff) {

	File *file;
	unsigned char *buffer;
	char *string, *fileString;
	int count, x, y;


	try {

		font = new Font(true);

	} catch (int e) {

		throw e;

	}

	try {

		file = new File(fileName, false);

	} catch (int e) {

		delete font;

		throw e;

	}

	// Load sprites
	loadSprites();


	// Load tileset

	file->seek(90, true);
	string = file->loadString();
	fileString = createFileName(string, 0);
	x = loadTiles(fileString);
	delete[] string;
	delete[] fileString;

	if (x != E_NONE) throw x;


	// Load music

	file->seek(121, true);
	fileString = file->loadString();
	playMusic(fileString);
	delete[] fileString;


	// Load animations

	file->seek(134, true);
	buffer = file->loadBlock(BANIMS << 6);

	// Create animation set based on that data
	for (count = 0; count < BANIMS; count++) {

		animSet[count].setData(buffer[(count << 6) + 6],
			buffer[count << 6], buffer[(count << 6) + 1],
			buffer[(count << 6) + 3], buffer[(count << 6) + 4],
			buffer[(count << 6) + 2], buffer[(count << 6) + 5]);

		for (y = 0; y < buffer[(count << 6) + 6]; y++) {

			// Get frame
			x = buffer[(count << 6) + 7 + y];
			if (x > sprites) x = sprites;

			// Assign sprite and vertical offset
			animSet[count].setFrame(y, true);
			animSet[count].setFrameData(spriteSet + x,
				buffer[(count << 6) + 26 + y], buffer[(count << 6) + 45 + y]);

		}

	}

	delete[] buffer;


	// Load tiles

	file->seek(2694, true);
	buffer = file->loadRLE(BLW * BLH);

	for (y = 0; y < BLH; y++) {

		for (x = 0; x < BLW; x++) {

			grid[y][x].tile = buffer[x + (y * BLW)];

			if (grid[y][x].tile > 59) grid[y][x].tile = 59;

		}

	}

	delete[] buffer;


	file->skipRLE(); // Mysterious block


	// Load events

	buffer = file->loadRLE(BLW * BLH);

	for (y = 0; y < BLW; y++) {

		for (x = 0; x < BLH; x++) {

			grid[y][x].event = buffer[x + (y * BLW)];

		}

	}

	delete[] buffer;


	file->seek(178, false);

	// Set the tick at which the level will end
	endTime = file->loadShort() * 1000;


	// Number of gems to collect
	items = file->loadShort();


	// The players' coordinates
	x = file->loadShort();
	y = file->loadShort();

	// Generate player's animation set references

	string = new char[PANIMS];

	for (count = 0; count < PANIMS; count++) string[count] = count & 31;

	// Set the players' initial values
	if (game) {

		game->setCheckpoint(x, y);

		for (count = 0; count < nPlayers; count++) game->resetPlayer(players + count, LT_BONUS, string);

	} else {

		localPlayer->reset(LT_BONUS, string, x, y);

	}

	delete[] string;



	delete file;


	// Palette animations

	// Spinny whirly thing
	paletteEffects = new RotatePaletteEffect(112, 16, F32, NULL);

	// Track sides
	paletteEffects = new RotatePaletteEffect(192, 16, F32, paletteEffects);

	// Bouncy things
	paletteEffects = new RotatePaletteEffect(240, 16, F32, paletteEffects);


	// Adjust panelBigFont to use bonus level palette
	panelBigFont->mapPalette(0, 32, 15, -16);


	return;

}


Bonus::~Bonus () {

	// Restore panelBigFont palette
	panelBigFont->restorePalette();

	SDL_FreeSurface(tileSet);

	delete font;

	return;

}


bool Bonus::isEvent (fixed x, fixed y) {

	return ((x & 32767) > F12) && ((x & 32767) < F20) &&
		((y & 32767) > F12) && ((y & 32767) < F20);

}


bool Bonus::checkMask (fixed x, fixed y) {

	BonusGridElement *ge;

	ge = grid[FTOT(y) & 255] + (FTOT(x) & 255);

	// Hand
	if ((ge->event == 3) && isEvent(x, y)) return true;

	// Check the mask in the tile in question
	return mask[ge->tile][((y >> 9) & 56) + ((x >> 12) & 7)];

}


void Bonus::receive (unsigned char* buffer) {

	// Interpret data received from client/server

	switch (buffer[1]) {

		case MT_L_PROP:

			if (buffer[2] == 2) {

				if (stage == LS_NORMAL)
					endTime += 2 * 60 * 1000; // 2 minutes. Is this right?

			}

			break;

		case MT_L_GRID:

			if (buffer[4] == 0) grid[buffer[3]][buffer[2]].tile = buffer[5];
			else if (buffer[4] == 2)
				grid[buffer[3]][buffer[2]].event = buffer[5];

			break;

		case MT_L_STAGE:

			stage = LevelStage(buffer[2]);

			break;

	}

	return;

}


int Bonus::step () {

	BonusPlayer* bonusPlayer;
	fixed playerX, playerY;
	int gridX, gridY;
	int msps;
	int count;

	// Milliseconds per step
	msps = ticks - prevStepTicks;
	prevStepTicks = ticks;


	// Check if time has run out
	if (ticks > endTime) return LOST;


	// Apply controls to local player
	for (count = 0; count < PCONTROLS; count++)
		localPlayer->setControl(count, controls.getState(count));

	// Process players
	for (count = 0; count < nPlayers; count++) {

		bonusPlayer = players[count].getBonusPlayer();

		playerX = bonusPlayer->getX();
		playerY = bonusPlayer->getY();

		bonusPlayer->step(ticks, msps, this);

		if (isEvent(playerX, playerY)) {

			gridX = FTOT(playerX) & 255;
			gridY = FTOT(playerY) & 255;

			switch (grid[gridY][gridX].event) {

				case 1: // Extra time

					addTimer();
					grid[gridY][gridX].event = 0;

					break;

				case 2: // Gem

					bonusPlayer->addGem();
					grid[gridY][gridX].event = 0;

					if (bonusPlayer->getGems() >= items) {

						players[count].addLife();

						return WON;

					}

					break;

				case 4: // Exit

					return LOST;

				default:

					// Do nothing

					break;

			}

		}

	}

	direction = localPlayer->getBonusPlayer()->getDirection();

	return E_NONE;

}


void Bonus::draw () {

	BonusPlayer *bonusPlayer;
	unsigned char* row;
	Sprite* sprite;
	SDL_Rect dst;
	fixed playerX, playerY, playerSin, playerCos;
	fixed distance, fwdX, fwdY, nX, sideX, sideY;
	int levelX, levelY;
	int x, y;


	// Draw the background

	for (x = -(direction & 1023); x < canvasW; x += background->w) {

		dst.x = x;
		dst.y = (canvasH >> 1) - 4;
		SDL_BlitSurface(background, NULL, canvas, &dst);

	}

	x = 171;

	for (y = (canvasH >> 1) - 5; (y >= 0) && (x > 128); y--) drawRect(0, y, canvasW, 1, x--);

	if (y > 0) drawRect(0, 0, canvasW, y + 1, 128);


	bonusPlayer = localPlayer->getBonusPlayer();


	// Draw the ground

	playerX = bonusPlayer->getX();
	playerY = bonusPlayer->getY();
	playerSin = fSin(direction);
	playerCos = fCos(direction);

	if (SDL_MUSTLOCK(canvas)) SDL_LockSurface(canvas);

	for (y = 1; y <= (canvasH >> 1) - 15; y++) {

		distance = DIV(ITOF(800), ITOF(92) - (ITOF(y * 84) / ((canvasH >> 1) - 16)));
		sideX = MUL(distance, playerCos);
		sideY = MUL(distance, playerSin);
		fwdX = playerX + MUL(distance - F16, playerSin) - (sideX >> 1);
		fwdY = playerY - MUL(distance - F16, playerCos) - (sideY >> 1);

		row = ((unsigned char *)(canvas->pixels)) + (canvas->pitch * (canvasH - y));

		for (x = 0; x < canvasW; x++) {

			nX = ITOF(x) / canvasW;

			levelX = FTOI(fwdX + MUL(nX, sideX));
			levelY = FTOI(fwdY + MUL(nX, sideY));

			row[x] = ((unsigned char *)(tileSet->pixels))
				[(grid[ITOT(levelY) & 255][ITOT(levelX) & 255].tile << 10) +
					((levelY & 31) * tileSet->pitch) + (levelX & 31)];

		}

	}

	if (SDL_MUSTLOCK(canvas)) SDL_UnlockSurface(canvas);


	// Draw nearby events

	for (y = -6; y < 6; y++) {

		fixed sY = TTOF(((direction - FQ) & 512)? y: -y) + F16 - (playerY & 32767);

		for (x = -6; x < 6; x++) {

			fixed sX = TTOF((direction & 512)? x: -x) + F16 - (playerX & 32767);

			fixed divisor = F16 + MUL(sX, playerSin) - MUL(sY, playerCos);

			if (FTOI(divisor) > 8) {

				switch (grid[((((direction - FQ) & 512)? y: -y) + FTOT(playerY)) & 255][(((direction & 512)? x: -x) + FTOT(playerX)) & 255].event) {

					case 0: // No event

						sprite = NULL;

						break;

					case 1: // Extra time

						sprite = spriteSet + 46;

						break;

					case 2: // Gem

						sprite = spriteSet + 47;

						break;

					case 3: // Hand

						sprite = spriteSet + 48;

						break;

					case 4: // Exit

						sprite = spriteSet + 49;

						break;

					default:

						sprite = spriteSet + 14;

						break;

				}

				if (sprite) {

					nX = DIV(MUL(sX, playerCos) + MUL(sY, playerSin), divisor);
					dst.x = FTOI(nX * canvasW) + (canvasW >> 1);
					dst.y = canvasH >> 1;
					sprite->drawScaled(dst.x, dst.y, DIV(F32, divisor));

				}

			}

		}

	}


	// Show the player
	bonusPlayer->draw(ticks, animSet);


	// Show gem count
	font->showString("*", 0, 0);
	font->showNumber(bonusPlayer->getGems() / 10, 50, 0);
	font->showNumber(bonusPlayer->getGems() % 10, 68, 0);
	font->showString("/", 65, 0);
	font->showNumber(items, 124, 0);


	// Show time remaining
	if (endTime > ticks) x = (endTime - ticks) / 1000;
	else x = 0;
	font->showNumber(x / 60, 250, 0);
	font->showString(":", 247, 0);
	font->showNumber((x / 10) % 6, 274, 0);
	font->showNumber(x % 10, 291, 0);


	return;

}


int Bonus::play () {

	const char* options[5] =
		{"continue game", "save game", "load game", "setup options", "quit game"};
	bool pmenu, pmessage;
	int option;
	unsigned int returnTime;
	int count;


	tickOffset = globalTicks;
	ticks = 16;
	prevStepTicks = 0;

	pmessage = pmenu = false;
	option = 0;

	returnTime = 0;

	video.setPalette(palette);

	while (true) {

		count = loop(pmenu, option, pmessage);

		if (count <= 0) return count;


		// Check if level has been won
		if (returnTime && (ticks > returnTime)) {

			if (localPlayer->getBonusPlayer()->getGems() >= items) {

				//if (playScene(F_BONUS_0SC) == E_QUIT) return E_QUIT;

				return WON;

			}

			return LOST;

		}


		// Process frame-by-frame activity

		if (!paused && (ticks >= prevStepTicks + 16) && (stage == LS_NORMAL)) {

			count = step();

			if (count < 0) return count;
			else if (count) {

				stage = LS_END;
				paletteEffects = new WhiteOutPaletteEffect(T_BONUS_END, paletteEffects);
				returnTime = ticks + T_BONUS_END;

			}

		}


		// Draw the graphics

		if ((ticks < returnTime) && !paused) direction += (ticks - prevTicks) * T_BONUS_END / (returnTime - ticks);

		draw();


		// If paused, draw "PAUSE"
		if (pmessage && !pmenu)
			font->showString("pause", (canvasW >> 1) - 44, 32);

		// Draw statistics
		drawStats(0);

		// Draw the menu
		if (pmenu) {

			// Draw the menu

			drawRect((canvasW >> 2) - 8, (canvasH >> 1) - 46, 144, 92, 0);

			for (count = 0; count < 5; count++) {

				if (count == option) fontmn2->mapPalette(240, 8, 31, 16);
				else fontmn2->mapPalette(240, 8, 0, 16);

				fontmn2->showString(options[count], canvasW >> 2, (canvasH >> 1) + (count << 4) - 38);

			}

			fontmn2->restorePalette();

		}

	}

	return E_NONE;

}


