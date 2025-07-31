#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "iGraphics.h"
#include "iSound.h"
#include <time.h>
#include <string.h>

#define WIDTH 1000
#define HEIGHT 650
#define MAX_LEADERBOARD 10
#define MAX_SAVED_GAMES 5

// Physics
const float jumpStrength = 2;
float birdX = WIDTH / 4;
float birdY;
float velocity = 0;
float gravity = -0.2;
float flapspeed = +5.0;
float horizontalSpeed = 10.0;

// Pipe
int pipeX[4];
int GapY[4];
int pipeGap = 225;
int pipeSpace = 300;
int pipeSpeed = 3;
int pipeCounter = 0;
bool useRedPipes = false;

// Game state
bool gameOver = false;
bool birdHit = false;
bool soundEnabled = true;
int score = 0;
int lives = 5;
int currentStreak = 0;

float gameSpeedMultiplier = 1.0;
float initialGameSpeedMultiplier = 1.0; // Persists user-set speed
float scoreMultiplier = 1.0;
int bgMusicChannel = -1;

bool isRespawning = false;
float respawnTimer = 0;
bool showSmash = false;
Image imgSmash;
int hitPipeIndex = -1;
int hitBlockerBirdIndex = -1;
int currentLeaderboardIndex = -1;
int loadedSlot = -1;

// Instructional message
bool showInstructions = false;
float instructionTimer = 0;

// Background toggle
bool useBackground2 = false;
Image imgbackground2;

// Countdown images
Image imgCountdown[5];

// UI
int bX = 400, bY = 350, bWidth = 200, bHeight = 50, bSpace = 20;
int musicVolume = 50;
enum GameState { MENU, INSTRUCTIONS, PLAYING, PAUSED, GAMEOVER, SETTINGS, LEADERBOARD, HELP, LOAD_GAME, ABOUT_US } gameState = MENU;
bool hoverNewGame = false, prevHoverNewGame = false;
bool hoverLoadGame = false, prevHoverLoadGame = false;
bool hoverExit = false, prevHoverExit = false;
bool hoverSettings = false, prevHoverSettings = false;
bool hoverLeaderboard = false, prevHoverLeaderboard = false;
bool hoverHelp = false, prevHoverHelp = false;
bool hoverAboutUs = false, prevHoverAboutUs = false;
bool hoverSavedGame[MAX_SAVED_GAMES] = {false};
bool prevHoverSavedGame[MAX_SAVED_GAMES] = {false};

char playerName[20] = "";
int nameCharIndex = 0;
bool enteringName = false;

// Assets
Image imgbackground, imgbase, imgpipe, imgpipeRed, imgGameover, imgDigits[10], frames[8];
Sprite birdSprite;
Sprite blockerBirdSprites[4];

// Sound
char sndPoint[] = "audio/point.wav";
char sndHit[] = "audio/HitSound.wav";
char sndWing[] = "audio/wing.wav";
char sndBackground[] = "audio/BackgroundSound.wav";
char sndHover[] = "audio/hover.wav";
char sndClick[] = "audio/click.wav";

typedef struct {
    float offsetY;
    float vy;
    bool active;
    int currentFrame;
} BlockerBird;
BlockerBird blockerBirds[4];

typedef struct {
    char name[20];
    int score;
} LeaderboardEntry;

LeaderboardEntry leaderboard[MAX_LEADERBOARD];

typedef struct {
    float birdX;
    float birdY;
    float velocity;
    int score;
    int lives;
    int currentStreak;
    float gameSpeedMultiplier;
    float scoreMultiplier;
    int pipeX[4];
    int GapY[4];
    BlockerBird blockerBirds[4];
    int pipeSpeed;
    bool valid;
    int pipeCounter;
    bool useRedPipes;
    bool useBackground2;
    int hitBlockerBirdIndex;
} SavedGame;

SavedGame savedGames[MAX_SAVED_GAMES];

bool finalHitAnimation = false;
int lastBirdFrame = -1;
float deltaTime = 16.0 / 1000.0;

void iMouseDrag(int x, int y) {}
void iMouseWheel(int dir, int x, int y) {}

void loadLeaderboard() {
    FILE *f = fopen("leaderboard.dat", "rb");
    if (f) {
        fread(leaderboard, sizeof(LeaderboardEntry), MAX_LEADERBOARD, f);
        fclose(f);
    } else {
        for (int i = 0; i < MAX_LEADERBOARD; i++) {
            strcpy(leaderboard[i].name, "---");
            leaderboard[i].score = 0;
        }
    }
}

void saveLeaderboard() {
    FILE *f = fopen("leaderboard.dat", "wb");
    if (f) {
        fwrite(leaderboard, sizeof(LeaderboardEntry), MAX_LEADERBOARD, f);
        fclose(f);
    }
}

void resetLeaderboard() {
    for (int i = 0; i < MAX_LEADERBOARD; i++) {
        strcpy(leaderboard[i].name, "---");
        leaderboard[i].score = 0;
    }
    saveLeaderboard();
    printf("Leaderboard reset.\n");
}

void loadSavedGames() {
    for (int i = 0; i < MAX_SAVED_GAMES; i++) {
        char filename[50];
        sprintf(filename, "saves/savegame%d.dat", i + 1);
        FILE *f = fopen(filename, "rb");
        if (f) {
            fread(&savedGames[i], sizeof(SavedGame), 1, f);
            fclose(f);
        } else {
            savedGames[i].valid = false;
        }
    }
}

void saveGame(int slot) {
    SavedGame save;
    save.birdX = birdX;
    save.birdY = birdY;
    save.velocity = velocity;
    save.score = score;
    save.lives = lives;
    save.currentStreak = currentStreak;
    save.gameSpeedMultiplier = gameSpeedMultiplier;
    save.scoreMultiplier = scoreMultiplier;
    save.pipeSpeed = pipeSpeed;
    save.pipeCounter = pipeCounter;
    save.useRedPipes = useRedPipes;
    save.useBackground2 = useBackground2;
    save.hitBlockerBirdIndex = hitBlockerBirdIndex;
    for (int i = 0; i < 4; i++) {
        save.pipeX[i] = pipeX[i];
        save.GapY[i] = GapY[i];
        save.blockerBirds[i] = blockerBirds[i];
    }
    save.valid = true;

    char filename[50];
    sprintf(filename, "saves/savegame%d.dat", slot + 1);
    FILE *f = fopen(filename, "wb");
    if (f) {
        fwrite(&save, sizeof(SavedGame), 1, f);
        fclose(f);
        printf("Game saved in slot %d\n", slot + 1);
    } else {
        printf("Failed to save game in slot %d\n", slot + 1);
    }
    savedGames[slot] = save;
}

void shiftSavedGames(int deletedSlot) {
    for (int i = deletedSlot; i < MAX_SAVED_GAMES - 1; i++) {
        savedGames[i] = savedGames[i + 1];
    }
    savedGames[MAX_SAVED_GAMES - 1].valid = false;

    for (int i = 0; i < MAX_SAVED_GAMES; i++) {
        char filename[50];
        sprintf(filename, "saves/savegame%d.dat", i + 1);
        if (savedGames[i].valid) {
            FILE *f = fopen(filename, "wb");
            if (f) {
                fwrite(&savedGames[i], sizeof(SavedGame), 1, f);
                fclose(f);
            } else {
                printf("Failed to write to %s\n", filename);
            }
        } else {
            remove(filename);
        }
    }
    printf("Save slot %d deleted, remaining slots shifted\n", deletedSlot + 1);
}

void loadGame(int slot) {
    if (savedGames[slot].valid) {
        birdX = savedGames[slot].birdX;
        birdY = savedGames[slot].birdY;
        velocity = savedGames[slot].velocity;
        score = savedGames[slot].score;
        lives = savedGames[slot].lives;
        currentStreak = savedGames[slot].currentStreak;
        gameSpeedMultiplier = initialGameSpeedMultiplier; // Use user-set speed
        scoreMultiplier = savedGames[slot].scoreMultiplier;
        pipeSpeed = 3 * initialGameSpeedMultiplier;
        pipeCounter = savedGames[slot].pipeCounter;
        useRedPipes = savedGames[slot].useRedPipes;
        useBackground2 = savedGames[slot].useBackground2;
        hitBlockerBirdIndex = savedGames[slot].hitBlockerBirdIndex;
        for (int i = 0; i < 4; i++) {
            pipeX[i] = savedGames[slot].pipeX[i];
            GapY[i] = savedGames[slot].GapY[i];
            blockerBirds[i] = savedGames[slot].blockerBirds[i];
            blockerBirdSprites[i].currentFrame = blockerBirds[i].currentFrame;
        }
        gameOver = false;
        birdHit = false;
        isRespawning = false;
        respawnTimer = 0;
        showSmash = false;
        hitPipeIndex = -1;
        finalHitAnimation = false;
        loadedSlot = slot;
        gameState = INSTRUCTIONS;
        showInstructions = true;
        instructionTimer = 0;
        lastBirdFrame = -1;
        printf("Loaded game from slot %d\n", slot + 1);
    }
}

void LoadResources() {
    if (!iLoadImage(&imgbackground, "sprites/BGIMAGE.png")) printf("Failed to load BGIMAGE.png\n");
    if (!iLoadImage(&imgbackground2, "sprites/BGIMAGE2.png")) printf("Failed to load BGIMAGE2.png\n");
    if (!iLoadImage(&imgbase, "sprites/base.png")) printf("Failed to load base.png\n");
    if (!iLoadImage(&imgpipe, "sprites/pipe-green.png")) printf("Failed to load pipe-green.png\n");
    if (!iLoadImage(&imgpipeRed, "sprites/pipe-red.png")) printf("Failed to load pipe-red.png\n");
    if (!iLoadImage(&imgGameover, "sprites/gameover.png")) printf("Failed to load gameover.png\n");
    if (!iLoadImage(&imgSmash, "sprites/smash.png")) printf("Failed to load smash.png\n");

    for (int i = 0; i < 5; i++) {
        char path[50];
        sprintf(path, "sprites/%d.png", i+1);
        if (!iLoadImage(&imgCountdown[i], path)) printf("Failed to load %s\n", path);
    }

    for (int i = 0; i < 10; i++) {
        char path[50];
        sprintf(path, "sprites/%d.png", i);
        if (!iLoadImage(&imgDigits[i], path)) printf("Failed to load %s\n", path);
    }

    for (int i = 0; i < 8; i++) {
        char path[50];
        sprintf(path, "sprites/frame-%d.png", i + 1);
        if (!iLoadImage(&frames[i], path)) printf("Failed to load %s\n", path);
    }

    iInitSprite(&birdSprite);
    iChangeSpriteFrames(&birdSprite, frames, 8);

    for (int i = 0; i < 4; i++) {
        iInitSprite(&blockerBirdSprites[i]);
        iChangeSpriteFrames(&blockerBirdSprites[i], frames, 8);
        iResizeSprite(&blockerBirdSprites[i], frames[0].width / 2, frames[0].height / 2);
    }
}

void initGame() {
    birdY = HEIGHT / 2;
    birdX = WIDTH / 4;
    velocity = 0;
    score = 0;
    lives = 5;
    gameOver = false;
    birdHit = false;
    isRespawning = false;
    respawnTimer = 0;
    showSmash = false;
    hitPipeIndex = -1;
    hitBlockerBirdIndex = -1;
    finalHitAnimation = false;
    gameSpeedMultiplier = initialGameSpeedMultiplier; // Use user-set speed
    scoreMultiplier = 1.0;
    currentStreak = 0;
    pipeSpeed = 3 * initialGameSpeedMultiplier;
    pipeCounter = 0;
    useRedPipes = false;
    enteringName = false;
    playerName[0] = '\0';
    nameCharIndex = 0;
    loadedSlot = -1;
    showInstructions = true;
    instructionTimer = 0;
    lastBirdFrame = -1;

    for (int i = 0; i < 4; i++) {
        if (i == 0)
            pipeX[i] = WIDTH / 2;
        else
            pipeX[i] = WIDTH + (i - 1) * pipeSpace;

        int minY = imgbase.height + 30;
        int maxY = HEIGHT - pipeGap - 30;
        GapY[i] = rand() % (maxY - minY + 1) + minY;
        blockerBirds[i].offsetY = pipeGap / 2;
        blockerBirds[i].vy = 1.0;
        blockerBirds[i].active = (i == 0);
        blockerBirds[i].currentFrame = 0;
        blockerBirdSprites[i].currentFrame = 0;
    }
}

bool checkCollision() {
    if (birdY <= imgbase.height || isRespawning) return false;

    Image* currentBirdFrame = &birdSprite.frames[birdSprite.currentFrame];
    int bw = currentBirdFrame->width;
    int bh = currentBirdFrame->height;

    if (birdSprite.currentFrame != lastBirdFrame) {
        iUpdateCollisionMask(&birdSprite);
        lastBirdFrame = birdSprite.currentFrame;
    }

    hitPipeIndex = -1;
    hitBlockerBirdIndex = -1;

    for (int i = 0; i < 4; i++) {
        if (pipeX[i] > birdX + 200 || pipeX[i] + imgpipe.width < birdX - 200) continue;

        int bx1 = birdX;
        int bx2 = birdX + bw;
        int by1 = birdY;
        int by2 = birdY + bh;

        int px1 = pipeX[i];
        int px2 = pipeX[i] + imgpipe.width;
        int gy = GapY[i];

        int pipeBottomY1 = gy - imgpipe.height;
        int pipeBottomY2 = gy;
        if (bx2 > px1 && bx1 < px2 && by2 > pipeBottomY1 && by1 < pipeBottomY2) {
            hitPipeIndex = i;
            return true;
        }

        int pipeTopY1 = gy + pipeGap;
        int pipeTopY2 = gy + pipeGap + imgpipe.height;
        if (bx2 > px1 && bx1 < px2 && by2 > pipeTopY1 && by1 < pipeTopY2) {
            hitPipeIndex = i;
            return true;
        }

        if (blockerBirds[i].active) {
            float blockerX = pipeX[i] + imgpipe.width / 2 - blockerBirdSprites[i].frames[blockerBirdSprites[i].currentFrame].width / 2;
            float blockerY = GapY[i] + blockerBirds[i].offsetY - blockerBirdSprites[i].frames[blockerBirdSprites[i].currentFrame].height / 2;

            int bx1_blocker = blockerX;
            int bx2_blocker = blockerX + blockerBirdSprites[i].frames[blockerBirdSprites[i].currentFrame].width;
            int by1_blocker = blockerY;
            int by2_blocker = blockerY + blockerBirdSprites[i].frames[blockerBirdSprites[i].currentFrame].height;

            int playerX1 = birdX;
            int playerX2 = birdX + currentBirdFrame->width;
            int playerY1 = birdY;
            int playerY2 = birdY + currentBirdFrame->height;

            if (playerX2 > bx1_blocker && playerX1 < bx2_blocker && playerY2 > by1_blocker && playerY1 < by2_blocker) {
                hitBlockerBirdIndex = i;
                printf("Collision with blocker bird %d!\n", i);
                return true;
            }
        }
    }
    return false;
}

void gameLoop() {
    static unsigned long lastTime = 0;
    unsigned long currentTime = glutGet(GLUT_ELAPSED_TIME);
    if (lastTime != 0) {
        deltaTime = (currentTime - lastTime) / 1000.0f;
    }
    lastTime = currentTime;

    if (gameState == INSTRUCTIONS) {
        instructionTimer += 16;
        if (instructionTimer >= 5000) {
            showInstructions = false;
            instructionTimer = 0;
            gameState = PLAYING;
        }
        return;
    }

    if (gameState != PLAYING) return;

    if (finalHitAnimation) {
        respawnTimer += 16;
        velocity += gravity * 2 * deltaTime * 60.0f;
        birdY += velocity * deltaTime * 60.0f;
        iAnimateSprite(&birdSprite);
        if (soundEnabled && respawnTimer == 16) iPlaySound(sndHit, false, 50);

        if (respawnTimer >= 2000) {
            finalHitAnimation = false;
            respawnTimer = 0;
            gameOver = true;
            gameState = GAMEOVER;
            if (loadedSlot != -1) {
                savedGames[loadedSlot].valid = false;
                shiftSavedGames(loadedSlot);
                loadedSlot = -1;
            }
            for (int i = 0; i < MAX_LEADERBOARD; i++) {
                if (score > leaderboard[i].score) {
                    for (int j = MAX_LEADERBOARD - 1; j > i; j--)
                        leaderboard[j] = leaderboard[j - 1];
                    strcpy(leaderboard[i].name, "");
                    leaderboard[i].score = score;
                    currentLeaderboardIndex = i;
                    enteringName = true;
                    playerName[0] = '\0';
                    nameCharIndex = 0;
                    break;
                }
            }
        }
        return;
    }

    if (gameOver) return;

    if (isRespawning) {
        respawnTimer += 16;
        if (respawnTimer < 1000) {
            showSmash = true;
            velocity += gravity * 2 * deltaTime * 60.0f;
            birdY += velocity * deltaTime * 60.0f;
            if (soundEnabled && respawnTimer == 16) iPlaySound(sndHit, false, 50);
        } else if (respawnTimer < 2000) {
            showSmash = false;
            velocity += gravity * 2 * deltaTime * 60.0f;
            birdY += velocity * deltaTime * 60.0f;
        } else {
            isRespawning = false;
            respawnTimer = 0;

            if (hitBlockerBirdIndex != -1 && birdX < WIDTH / 5) {
                birdX = pipeX[hitBlockerBirdIndex] + imgpipe.width + 100;
                birdY = GapY[hitBlockerBirdIndex] + pipeGap / 2 - (birdSprite.frames[0].height / 2);

                if (birdX < 50) birdX = 50;
                if (birdX > WIDTH - birdSprite.frames[0].width - 50) birdX = WIDTH - birdSprite.frames[0].width - 50;

                if (birdY < imgbase.height + 50) birdY = imgbase.height + 50;
                if (birdY > HEIGHT - birdSprite.frames[0].height - 50) birdY = HEIGHT - birdSprite.frames[0].height - 50;

                printf("Respawning after blocker bird %d at x=%f, y=%f\n", hitBlockerBirdIndex, birdX, birdY);
            } else {
                int closestPipeIndex = -1;
                float minDistance = 99999;
                for (int i = 0; i < 4; i++) {
                    if (pipeX[i] + imgpipe.width > birdX + birdSprite.frames[0].width) {
                        float distance = (pipeX[i] + imgpipe.width / 2) - birdX;
                        if (distance < minDistance) {
                            minDistance = distance;
                            closestPipeIndex = i;
                        }
                    }
                }
                if (closestPipeIndex != -1) {
                    birdX = pipeX[closestPipeIndex] - 250;
                    if (birdX < 50) birdX = 50;
                    if (birdX > WIDTH - birdSprite.frames[0].width - 50) birdX = WIDTH - birdSprite.frames[0].width - 50;
                    birdY = GapY[closestPipeIndex] + pipeGap / 2 - (birdSprite.frames[0].height / 2);
                    if (birdY < imgbase.height + 50) birdY = imgbase.height + 50;
                    if (birdY > HEIGHT - birdSprite.frames[0].height - 50) birdY = HEIGHT - birdSprite.frames[0].height - 50;
                } else {
                    birdX = WIDTH / 4;
                    birdY = HEIGHT / 2;
                }
            }
            velocity = 0;
            hitPipeIndex = -1;
            hitBlockerBirdIndex = -1;
            showSmash = false;
        }
        return;
    }

    velocity += gravity * deltaTime * 60.0f;
    birdY += velocity * deltaTime * 60.0f;
    iAnimateSprite(&birdSprite);

    for (int i = 0; i < 4; i++) {
        if (blockerBirds[i].active) {
            iAnimateSprite(&blockerBirdSprites[i]);
            blockerBirds[i].currentFrame = blockerBirdSprites[i].currentFrame;
        }
    }

    for (int i = 0; i < 4; i++) pipeX[i] -= pipeSpeed * deltaTime * 60.0f;

    int maxX = pipeX[0];
    for (int j = 1; j < 4; j++) if (pipeX[j] > maxX) maxX = pipeX[j];
    for (int i = 0; i < 4; i++) {
        if (pipeX[i] + imgpipe.width < 0) {
            pipeX[i] = maxX + pipeSpace;
            int minY = imgbase.height + 30;
            int maxY = HEIGHT - pipeGap - 30;
            GapY[i] = rand() % (maxY - minY + 1) + minY;
            if (pipeCounter % 3 == 0 && pipeCounter > 0) { // Blocker bird every 3 pipes
                blockerBirds[i].offsetY = pipeGap / 2;
                blockerBirds[i].vy = 1.0;
                blockerBirds[i].active = true;
                blockerBirds[i].currentFrame = 0;
                blockerBirdSprites[i].currentFrame = 0;
            } else {
                blockerBirds[i].active = false;
            }
            pipeCounter++;
            if (pipeCounter >= 9) {
                useRedPipes = !useRedPipes;
                pipeCounter = 0;
            }
        }
        int bw = birdSprite.frames[0].width;
        if (birdX + bw > pipeX[i] + imgpipe.width && birdX + bw - pipeSpeed * deltaTime * 60.0f <= pipeX[i] + imgpipe.width) {
            score++;
            currentStreak++;
            if (currentStreak % 9 == 0) {
                gameSpeedMultiplier += 0.1; // Increase by 0.1x
                if (gameSpeedMultiplier > 2.0) gameSpeedMultiplier = 2.0; // Cap at 2.0x
                pipeSpeed = 3 * gameSpeedMultiplier;
                scoreMultiplier *= 2;
            }
            int scoreIncrement = 1 * scoreMultiplier;
            score += scoreIncrement - 1;
            if (score > 0 && score % 100 == 0 && lives < 5) {
                lives++;
                printf("Extra life gained! Total lives: %d\n", lives);
            }
            if (soundEnabled) iPlaySound(sndPoint, false, 50);
        }
    }

    if (birdY + birdSprite.frames[0].height <= 0 || checkCollision()) {
        lives--;
        if (lives <= 0) {
            finalHitAnimation = true;
            respawnTimer = 0;
            showSmash = true;
            velocity = jumpStrength;
            scoreMultiplier = 1.0;
            currentStreak = 0;
            if (soundEnabled) iPlaySound(sndHit, false, 50);
            printf("Final hit: playing animation before GAME OVER\n");
        } else {
            isRespawning = true;
            showSmash = true;
            respawnTimer = 0;
            velocity = jumpStrength;
            scoreMultiplier = 1.0;
            currentStreak = 0;
            if (soundEnabled) iPlaySound(sndHit, false, 50);
            printf("Collision or fell, lives left: %d, respawning...\n", lives);
        }
    }

    for (int i = 0; i < 4; i++) {
        if (blockerBirds[i].active) {
            blockerBirds[i].offsetY += blockerBirds[i].vy * deltaTime * 60.0f;
            if (blockerBirds[i].offsetY > pipeGap - 20 || blockerBirds[i].offsetY < 20) {
                blockerBirds[i].vy *= -1;
            }
        }
    }
}

void iDraw() {
    iClear();
    iShowLoadedImage(0, 0, useBackground2 ? &imgbackground2 : &imgbackground);

    if (gameState == MENU) {
        iSetColor(useBackground2 ? 0 : 46, useBackground2 ? 0 : 26, useBackground2 ? 0 : 120);
        iTextAdvanced(300, 550, "FLAPPY BIRD", 0.5, 2.0, GLUT_STROKE_ROMAN);

        iSetColor(220, 109, 27);
        iFilledRectangle(bX, bY+50, bWidth, bHeight);
        iFilledRectangle(bX, bY - (bHeight + bSpace)+50, bWidth, bHeight);
        iFilledRectangle(bX, bY - 2 * (bHeight + bSpace)+50, bWidth, bHeight);
        iFilledRectangle(bX, bY - 3 * (bHeight + bSpace)+50, bWidth, bHeight);
        iFilledRectangle(bX, bY - 4 * (bHeight + bSpace)+50, bWidth, bHeight);
        iFilledRectangle(bX, bY - 5 * (bHeight + bSpace)+50, bWidth, bHeight);

        iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(bX + 30, bY + 65, "NEW GAME", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(bX + 25, bY - (bHeight + bSpace) + 65, "LOAD GAME", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(bX + 70, bY - 2 * (bHeight + bSpace) + 65, "EXIT", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(bX + 40, bY - 3 * (bHeight + bSpace) + 65, "SETTINGS", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(bX + 10, bY - 4 * (bHeight + bSpace) + 65, "LEADERBOARD", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(bX + 30, bY - 5 * (bHeight + bSpace) + 65, "ABOUT US", 0.2, 2.0, GLUT_STROKE_ROMAN);

        iSetColor(0, 255, 0);
        iFilledRectangle(WIDTH - 100, 0, 100, 50);
        iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(WIDTH - 80, 15, "HELP", 0.2, 2.0, GLUT_STROKE_ROMAN);

        if (hoverNewGame) iSetColor(useBackground2 ? 128 : 192, useBackground2 ? 128 : 192, useBackground2 ? 128 : 192); else iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(bX + 30, bY + 65, "NEW GAME", 0.2, 2.0, GLUT_STROKE_ROMAN);
        if (hoverLoadGame) iSetColor(useBackground2 ? 128 : 192, useBackground2 ? 128 : 192, useBackground2 ? 128 : 192); else iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(bX + 25, bY - (bHeight + bSpace) + 65, "LOAD GAME", 0.2, 2.0, GLUT_STROKE_ROMAN);
        if (hoverExit) iSetColor(useBackground2 ? 128 : 192, useBackground2 ? 128 : 192, useBackground2 ? 128 : 192); else iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(bX + 70, bY - 2 * (bHeight + bSpace) + 65, "EXIT", 0.2, 2.0, GLUT_STROKE_ROMAN);
        if (hoverSettings) iSetColor(useBackground2 ? 128 : 192, useBackground2 ? 128 : 192, useBackground2 ? 128 : 192); else iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(bX + 40, bY - 3 * (bHeight + bSpace) + 65, "SETTINGS", 0.2, 2.0, GLUT_STROKE_ROMAN);
        if (hoverLeaderboard) iSetColor(useBackground2 ? 128 : 192, useBackground2 ? 128 : 192, useBackground2 ? 128 : 192); else iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(bX + 10, bY - 4 * (bHeight + bSpace) + 65, "LEADERBOARD", 0.2, 2.0, GLUT_STROKE_ROMAN);
        if (hoverAboutUs) iSetColor(useBackground2 ? 128 : 192, useBackground2 ? 128 : 192, useBackground2 ? 128 : 192); else iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(bX + 30, bY - 5 * (bHeight + bSpace) + 65, "ABOUT US", 0.2, 2.0, GLUT_STROKE_ROMAN);
        if (hoverHelp) iSetColor(useBackground2 ? 128 : 192, useBackground2 ? 128 : 192, useBackground2 ? 128 : 192); else iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(WIDTH - 80, 15, "HELP", 0.2, 2.0, GLUT_STROKE_ROMAN);
    } else if (gameState == INSTRUCTIONS) {
        iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(WIDTH / 2 - 220, HEIGHT / 2 + 80, "Press 'Space bar' or 'W' to go up", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(WIDTH / 2 - 220, HEIGHT / 2 + 40, "Press 'A'/'D' to go left/right", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(WIDTH / 2 - 220, HEIGHT / 2, "Press 'P' to pause the game", 0.2, 2.0, GLUT_STROKE_ROMAN);
        int countdownIndex = 4 - (int)(instructionTimer / 1000);
        if (countdownIndex >= 0 && countdownIndex < 5) {
            iShowLoadedImage(WIDTH / 2 - imgCountdown[countdownIndex].width / 2, HEIGHT / 2 - 100, &imgCountdown[countdownIndex]);
        }
    } else if (gameState == PLAYING) {
        for (int i = 0; i < 4; i++) {
            Image* currentPipe = useRedPipes ? &imgpipeRed : &imgpipe;
            iShowLoadedImage(pipeX[i], GapY[i] + pipeGap, currentPipe, -1, -1, VERTICAL);
            iShowLoadedImage(pipeX[i], GapY[i] - currentPipe->height, currentPipe);
        }
        iShowLoadedImage(0, 0, &imgbase);
        iShowLoadedImage(288, 0, &imgbase);
        iShowLoadedImage(576, 0, &imgbase);
        iShowLoadedImage(864, 0, &imgbase);

        if (showSmash) {
            iShowLoadedImage(birdX, birdY, &birdSprite.frames[birdSprite.currentFrame]);
            iShowLoadedImage(birdX, birdY, &imgSmash);
        } else {
            iSetSpritePosition(&birdSprite, birdX, birdY);
            iShowSprite(&birdSprite);
        }
        iSetColor(255, 165, 0);
        char scoreStr[20];
        sprintf(scoreStr, "Score: %d", score);
        iTextAdvanced(18, HEIGHT - 30, scoreStr, 0.2, 2.0, GLUT_STROKE_ROMAN);

        char livesStr[20];
        sprintf(livesStr, "Lives: %d", lives);
        iTextAdvanced(20, HEIGHT - 60, livesStr, 0.2, 2.0, GLUT_STROKE_ROMAN);

        for (int i = 0; i < 4; i++) {
            if (blockerBirds[i].active) {
                float blockerX = pipeX[i] + imgpipe.width / 2 - blockerBirdSprites[i].frames[blockerBirdSprites[i].currentFrame].width / 2;
                float blockerY = GapY[i] + blockerBirds[i].offsetY - blockerBirdSprites[i].frames[blockerBirdSprites[i].currentFrame].height / 2;
                iSetSpritePosition(&blockerBirdSprites[i], blockerX, blockerY);
                iShowSprite(&blockerBirdSprites[i]);
            }
        }
    } else if (gameState == GAMEOVER) {
        iShowLoadedImage((WIDTH - imgGameover.width) / 2, (HEIGHT - imgGameover.height) / 1.2, &imgGameover);
        iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        char finalScoreStr[50];
        sprintf(finalScoreStr, "Final Score: %d", score);
        iTextAdvanced(WIDTH / 2 - 110, HEIGHT / 2 + 100, finalScoreStr, 0.2, 2.0, GLUT_STROKE_ROMAN);
        if (enteringName) {
            iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
            char highScoreStr[50];
            sprintf(highScoreStr, "New %d%s High Score! Enter your name:", currentLeaderboardIndex + 1,
                   (currentLeaderboardIndex + 1 == 1) ? "st" :
                   (currentLeaderboardIndex + 1 == 2) ? "nd" :
                   (currentLeaderboardIndex + 1 == 3) ? "rd" : "th");
            iTextAdvanced(WIDTH / 2 - 200, HEIGHT / 2 + 40, highScoreStr, 0.2, 2.0, GLUT_STROKE_ROMAN);
            iTextAdvanced(WIDTH / 2 - 100, HEIGHT / 2.2, playerName, 0.3, 2.0, GLUT_STROKE_ROMAN);
            iTextAdvanced(WIDTH / 2 - 100, HEIGHT / 2 - 100, "(Press Enter to confirm)", 0.15, 2.0, GLUT_STROKE_ROMAN);
        } else {
            iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
            iTextAdvanced(WIDTH / 2 - 250, HEIGHT / 2 - 90, "Press 'R' to Restart or 'M' for Menu", 0.2, 2.0, GLUT_STROKE_ROMAN);
        }
    } else if (gameState == PAUSED) {
        iSetColor(255, 255, 0);
        iTextAdvanced(WIDTH / 2 - 100, HEIGHT / 1.5, "Paused", 0.4, 2.0, GLUT_STROKE_ROMAN);
        iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(WIDTH / 2 - 250, HEIGHT / 1.5 - 60, "Press 'R' to Restart or ESC to Exit", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(WIDTH / 2 - 250, HEIGHT / 1.5 - 100, "Press 'C' to Continue or 'M' for Menu", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(WIDTH / 2 - 250, HEIGHT / 1.5 - 140, "Press 'S' to Save and return to Menu", 0.2, 2.0, GLUT_STROKE_ROMAN);
    } else if (gameState == SETTINGS) {
        iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(350, 550, "SETTINGS", 0.5, 2.0, GLUT_STROKE_ROMAN);
        iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(250, 450, soundEnabled ? "Sound: ON (Press S to toggle)" : "Sound: OFF (Press S to toggle)", 0.2, 2.0, GLUT_STROKE_ROMAN);
        char volumeStr[50];
        sprintf(volumeStr, "Music Volume: %d (Press UP/DOWN to adjust)", musicVolume);
        iTextAdvanced(250, 400, volumeStr, 0.2, 2.0, GLUT_STROKE_ROMAN);
        char speedStr[50];
        sprintf(speedStr, "Game Speed: %.1fx (Press LEFT/RIGHT to adjust)", initialGameSpeedMultiplier);
        iTextAdvanced(250, 350, speedStr, 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(250, 300, useBackground2 ? "Background: BGIMAGE2 (Press B to toggle)" : "Background: BGIMAGE (Press B to toggle)", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iSetColor(255, 0, 0);
        iTextAdvanced(250, 250, "Press M to return to Menu", 0.2, 2.0, GLUT_STROKE_ROMAN);
    } else if (gameState == LEADERBOARD) {
        iSetColor(255, 255, 0);
        iTextAdvanced(310, 550, "LEADERBOARD", 0.4, 2.0, GLUT_STROKE_ROMAN);
        iSetColor(255, 255, 255);
        for (int i = 0; i < MAX_LEADERBOARD; i++) {
            char entryStr[50];
            sprintf(entryStr, "%d. %s - %d", i + 1, leaderboard[i].name, leaderboard[i].score);
            iTextAdvanced(350, 480 - i * 40, entryStr, 0.2, 2.0, GLUT_STROKE_ROMAN);
        }
        iTextAdvanced(300, 50, "Press M to return to the Menu", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(290, 25, "Press R to reset the Leaderboard", 0.2, 2.0, GLUT_STROKE_ROMAN);
    } else if (gameState == HELP) {
        iSetColor(255, 255, 0);
        iTextAdvanced(WIDTH / 2 - 100, HEIGHT / 2 + 150, "Instructions", 0.3, 2.0, GLUT_STROKE_ROMAN);
        iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(WIDTH / 2 - 250, HEIGHT / 2 + 80, "- Press Space or W to jump.", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(WIDTH / 2 - 250, HEIGHT / 2 + 40, "- Use A and D to move left/right.", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(WIDTH / 2 - 250, HEIGHT / 2, "- Avoid pipes and blocker birds.", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(WIDTH / 2 - 250, HEIGHT / 2 - 40, "- +1 life every 100 points (max 5).", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(WIDTH / 2 - 250, HEIGHT / 2 - 80, "- Press P to pause, M for menu.", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(WIDTH / 2 - 250, HEIGHT / 2 - 120, "- Speed increases by 0.1x after every 9 pipes.", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(300, 50, "Press M to return to the Menu", 0.2, 2.0, GLUT_STROKE_ROMAN);
    } else if (gameState == LOAD_GAME) {
        iSetColor(255, 255, 0);
        iTextAdvanced(bX - 150, 550, "LOAD SAVED GAMES", 0.4, 2.0, GLUT_STROKE_ROMAN);
        for (int i = 0; i < MAX_SAVED_GAMES; i++) {
            char saveStr[50];
            if (savedGames[i].valid) {
                sprintf(saveStr, "Slot %d: Score %d", i + 1, savedGames[i].score);
            } else {
                sprintf(saveStr, "Slot %d: Empty", i + 1);
            }
            iSetColor(220, 109, 27);
            iFilledRectangle(bX - 50, bY - i * (bHeight + bSpace) + 50, bWidth + 100, bHeight);
            if (hoverSavedGame[i]) iSetColor(192, 192, 192); else iSetColor(255, 255, 255);
            iTextAdvanced(bX - 30, bY - i * (bHeight + bSpace) + 65, saveStr, 0.2, 2.0, GLUT_STROKE_ROMAN);
        }
        iSetColor(255, 0, 0);
        iTextAdvanced(bX - 80, 50, "Press M to return to the Menu", 0.2, 2.0, GLUT_STROKE_ROMAN);
    } else if (gameState == ABOUT_US) {
        iSetColor(255, 255, 0);
        iTextAdvanced(350, 550, "ABOUT US", 0.4, 2.0, GLUT_STROKE_ROMAN);
        iSetColor(255, 255, 0);
        iTextAdvanced(200, 480, "Supervisor:", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(350, 480, "Dr. A.K.M. Ashikur Rahman, Professor, BUET", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iSetColor(255, 255, 0);
        iTextAdvanced(200, 440, "Developers:", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(220, 400, "Rajdip Deb (ID: 2405122)", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(220, 360, "Sheikh Shabab Ahmed (ID: 2405143)", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(200, 320, "Dept.: CSE, Section: C1, Group: 02, Level-1, Term-1", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iSetColor(255, 255, 0);
        iTextAdvanced(200, 280, "iGraphics Contributors:", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iSetColor(useBackground2 ? 0 : 255, useBackground2 ? 0 : 255, useBackground2 ? 0 : 255);
        iTextAdvanced(220, 240, "Shahriar Nirjon, Mahir Labib Dihan", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(220, 200, "Anwarul Bashar Shuaib, Md. Ashrafur Rahman Khan", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iTextAdvanced(200, 160, "This gaming project was developed using iGraphics.h", 0.2, 2.0, GLUT_STROKE_ROMAN);
        iSetColor(255, 0, 0);
        iTextAdvanced(300, 50, "Press M to return to the Menu", 0.2, 2.0, GLUT_STROKE_ROMAN);
    }
}

void iMouse(int button, int state, int mx, int my) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && !isRespawning && !finalHitAnimation) {
        if (gameState == MENU) {
            if (mx >= bX && mx <= bX + bWidth) {
                if (my >= bY + 50 && my <= bY + 50 + bHeight) {
                    if (soundEnabled) iPlaySound(sndClick, false, 50);
                    initGame();
                    gameState = INSTRUCTIONS;
                } else if (my >= bY - 1 * (bHeight + bSpace) + 50 && my <= bY - 1 * (bHeight + bSpace) + 50 + bHeight) {
                    if (soundEnabled) iPlaySound(sndClick, false, 50);
                    loadSavedGames();
                    gameState = LOAD_GAME;
                } else if (my >= bY - 2 * (bHeight + bSpace) + 50 && my <= bY - 2 * (bHeight + bSpace) + 50 + bHeight) {
                    if (soundEnabled) iPlaySound(sndClick, false, 50);
                    exit(0);
                } else if (my >= bY - 3 * (bHeight + bSpace) + 50 && my <= bY - 3 * (bHeight + bSpace) + 50 + bHeight) {
                    if (soundEnabled) iPlaySound(sndClick, false, 50);
                    gameState = SETTINGS;
                } else if (my >= bY - 4 * (bHeight + bSpace) + 50 && my <= bY - 4 * (bHeight + bSpace) + 50 + bHeight) {
                    if (soundEnabled) iPlaySound(sndClick, false, 50);
                    gameState = LEADERBOARD;
                } else if (my >= bY - 5 * (bHeight + bSpace) + 50 && my <= bY - 5 * (bHeight + bSpace) + 50 + bHeight) {
                    if (soundEnabled) iPlaySound(sndClick, false, 50);
                    gameState = ABOUT_US;
                }
            } else if (my >= 0 && my <= 50 && mx >= WIDTH - 100 && mx <= WIDTH) {
                if (soundEnabled) iPlaySound(sndClick, false, 50);
                gameState = HELP;
            }
        } else if (gameState == LOAD_GAME) {
            for (int i = 0; i < MAX_SAVED_GAMES; i++) {
                if (mx >= bX - 50 && mx <= bX + bWidth + 50 && my >= bY - i * (bHeight + bSpace) + 50 && my <= bY - i * (bHeight + bSpace) + 50 + bHeight) {
                    if (savedGames[i].valid) {
                        if (soundEnabled) iPlaySound(sndClick, false, 50);
                        loadGame(i);
                    }
                }
            }
        }
    }
}

void iMouseMove(int mx, int my) {
    if (gameState == MENU) {
        bool newHoverNewGame = (mx >= bX && mx <= bX + bWidth && my >= bY + 50 && my <= bY + 50 + bHeight);
        bool newHoverLoadGame = (mx >= bX && mx <= bX + bWidth && my >= bY - 1 * (bHeight + bSpace) + 50 && my <= bY - 1 * (bHeight + bSpace) + 50 + bHeight);
        bool newHoverExit = (mx >= bX && mx <= bX + bWidth && my >= bY - 2 * (bHeight + bSpace) + 50 && my <= bY - 2 * (bHeight + bSpace) + 50 + bHeight);
        bool newHoverSettings = (mx >= bX && mx <= bX + bWidth && my >= bY - 3 * (bHeight + bSpace) + 50 && my <= bY - 3 * (bHeight + bSpace) + 50 + bHeight);
        bool newHoverLeaderboard = (mx >= bX && mx <= bX + bWidth && my >= bY - 4 * (bHeight + bSpace) + 50 && my <= bY - 4 * (bHeight + bSpace) + 50 + bHeight);
        bool newHoverAboutUs = (mx >= bX && mx <= bX + bWidth && my >= bY - 5 * (bHeight + bSpace) + 50 && my <= bY - 5 * (bHeight + bSpace) + 50 + bHeight);
        bool newHoverHelp = (my >= 0 && my <= 50 && mx >= WIDTH - 100 && mx <= WIDTH);

        if (soundEnabled) {
            if (newHoverNewGame && !prevHoverNewGame) iPlaySound(sndHover, false, 50);
            if (newHoverLoadGame && !prevHoverLoadGame) iPlaySound(sndHover, false, 50);
            if (newHoverExit && !prevHoverExit) iPlaySound(sndHover, false, 50);
            if (newHoverSettings && !prevHoverSettings) iPlaySound(sndHover, false, 50);
            if (newHoverLeaderboard && !prevHoverLeaderboard) iPlaySound(sndHover, false, 50);
            if (newHoverAboutUs && !prevHoverAboutUs) iPlaySound(sndHover, false, 50);
            if (newHoverHelp && !prevHoverHelp) iPlaySound(sndHover, false, 50);
        }

        hoverNewGame = newHoverNewGame;
        hoverLoadGame = newHoverLoadGame;
        hoverExit = newHoverExit;
        hoverSettings = newHoverSettings;
        hoverLeaderboard = newHoverLeaderboard;
        hoverAboutUs = newHoverAboutUs;
        hoverHelp = newHoverHelp;

        prevHoverNewGame = newHoverNewGame;
        prevHoverLoadGame = newHoverLoadGame;
        prevHoverExit = newHoverExit;
        prevHoverSettings = newHoverSettings;
        prevHoverLeaderboard = newHoverLeaderboard;
        prevHoverAboutUs = newHoverAboutUs;
        prevHoverHelp = newHoverHelp;
    } else if (gameState == LOAD_GAME) {
        for (int i = 0; i < MAX_SAVED_GAMES; i++) {
            bool newHoverSavedGame = (mx >= bX - 50 && mx <= bX + bWidth + 50 && my >= bY - i * (bHeight + bSpace) + 50 && my <= bY - i * (bHeight + bSpace) + 50 + bHeight);
            if (soundEnabled && newHoverSavedGame && !prevHoverSavedGame[i]) {
                iPlaySound(sndHover, false, 50);
            }
            hoverSavedGame[i] = newHoverSavedGame;
            prevHoverSavedGame[i] = newHoverSavedGame;
        }
    }
}

void iKeyboard(unsigned char key) {
    if (gameState == GAMEOVER && enteringName) {
        if (key == 13) {
            enteringName = false;
            if (currentLeaderboardIndex != -1) {
                strcpy(leaderboard[currentLeaderboardIndex].name, playerName);
                saveLeaderboard();
                currentLeaderboardIndex = -1;
            }
        } else if (key == 8) {
            if (nameCharIndex > 0) {
                nameCharIndex--;
                playerName[nameCharIndex] = '\0';
            }
        } else if (nameCharIndex < 19 && key >= 32 && key <= 126) {
            playerName[nameCharIndex++] = key;
            playerName[nameCharIndex] = '\0';
        }
        return;
    }

    if (gameState == GAMEOVER) {
        switch(key) {
            case 'R':
            case 'r': {
                initGame();
                gameState = INSTRUCTIONS;
                break;
            }
            case 'M':
            case 'm': {
                gameState = MENU;
                break;
            }
            default: {
                break;
            }
        }
    } else if (gameState == PLAYING && !isRespawning && !finalHitAnimation) {
        switch(key) {
            case ' ':
            case 'W':
            case 'w': {
                velocity = jumpStrength * 2;
                if (soundEnabled) iPlaySound(sndWing, false, 50);
                break;
            }
            case 'P':
            case 'p': {
                gameState = PAUSED;
                break;
            }
            case 'A':
            case 'a': {
                birdX -= horizontalSpeed;
                if (birdX < 0) birdX = 0;
                break;
            }
            case 'D':
            case 'd': {
                birdX += horizontalSpeed;
                if (birdX > WIDTH - birdSprite.frames[0].width)
                    birdX = WIDTH - birdSprite.frames[0].width;
                break;
            }
            default: {
                break;
            }
        }
    } else if (gameState == PAUSED) {
        switch(key) {
            case 'R':
            case 'r': {
                initGame();
                gameState = INSTRUCTIONS;
                break;
            }
            case 'M':
            case 'm': {
                gameState = MENU;
                break;
            }
            case 'C':
            case 'c': {
                gameState = PLAYING;
                break;
            }
            case 'S':
            case 's': {
                loadSavedGames();
                int slot = (loadedSlot != -1) ? loadedSlot : -1;
                if (slot == -1) {
                    for (int i = 0; i < MAX_SAVED_GAMES; i++) {
                        if (!savedGames[i].valid) {
                            slot = i;
                            break;
                        }
                    }
                    if (slot == -1) slot = 0;
                }
                saveGame(slot);
                gameState = MENU;
                break;
            }
            default: {
                break;
            }
        }
    } else if (gameState == SETTINGS) {
        switch(key) {
            case 'S':
            case 's': {
                soundEnabled = !soundEnabled;
                if (!soundEnabled) iStopSound(bgMusicChannel);
                else iPlaySound(sndBackground, true, musicVolume);
                break;
            }
            case 'B':
            case 'b': {
                useBackground2 = !useBackground2;
                break;
            }
            case 'M':
            case 'm': {
                gameState = MENU;
                break;
            }
            default: {
                break;
            }
        }
    } else if (gameState == LEADERBOARD) {
        switch(key) {
            case 'M':
            case 'm': {
                gameState = MENU;
                break;
            }
            case 'R':
            case 'r': {
                resetLeaderboard();
                break;
            }
            default: {
                break;
            }
        }
    } else if (gameState == HELP) {
        switch(key) {
            case 'M':
            case 'm': {
                gameState = MENU;
                break;
            }
            default: {
                break;
            }
        }
    } else if (gameState == LOAD_GAME) {
        switch(key) {
            case 'M':
            case 'm': {
                gameState = MENU;
                break;
            }
            default: {
                break;
            }
        }
    } else if (gameState == ABOUT_US) {
        switch(key) {
            case 'M':
            case 'm': {
                gameState = MENU;
                break;
            }
            default: {
                break;
            }
        }
    }
    if (key == 27) exit(0);
}

void iSpecialKeyboard(unsigned char key) {
    if (gameState == PLAYING && !isRespawning && !finalHitAnimation) {
        if (key == ' ') {
            velocity = flapspeed;
            if (soundEnabled) iPlaySound(sndWing, false, 50);
        }
    }
    if (gameState == SETTINGS) {
        if (key == GLUT_KEY_UP && musicVolume < 100) {
            musicVolume += 2;
            Mix_Volume(bgMusicChannel, musicVolume * MIX_MAX_VOLUME / 100);
        } else if (key == GLUT_KEY_DOWN && musicVolume > 0) {
            musicVolume -= 2;
            Mix_Volume(bgMusicChannel, musicVolume * MIX_MAX_VOLUME / 100);
        } else if (key == GLUT_KEY_RIGHT && initialGameSpeedMultiplier < 2.0) {
            initialGameSpeedMultiplier += 0.1;
            if (initialGameSpeedMultiplier > 2.0) initialGameSpeedMultiplier = 2.0;
        } else if (key == GLUT_KEY_LEFT && initialGameSpeedMultiplier > 0.5) {
            initialGameSpeedMultiplier -= 0.1;
            if (initialGameSpeedMultiplier < 0.5) initialGameSpeedMultiplier = 0.5;
        }
    }
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    srand(time(0));
    initialGameSpeedMultiplier = 1.0; // Reset to 1.0 on program start
    initGame();
    LoadResources();
    iSetTimer(16, gameLoop);
    iInitializeSound();
    iPlaySound(sndBackground, true, musicVolume);
    loadLeaderboard();
    loadSavedGames();
    iInitialize(WIDTH, HEIGHT, "Flappy Bird with Menu and States");
    return 0;
}
