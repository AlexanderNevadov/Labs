#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include "stb_easy_font.h"

#define WIDTH 25
#define HEIGHT 25
#define CELL_SIZE 30

#define SAFE_MARGIN 3

#define START_SPEED 0.2

#define SIDEBAR_WIDTH 220
#define WINDOW_WIDTH (WIDTH * CELL_SIZE + SIDEBAR_WIDTH)
#define WINDOW_HEIGHT (HEIGHT * CELL_SIZE)

#define MAX_LENGTH 100

typedef enum
{
    UP,
    DOWN,
    LEFT,
    RIGHT
} Direction;

typedef enum // уровень сложности
{
    LEVEL_WRAP, // через края (телепорт)
    LEVEL_WALLS, // стенки воркуг поля
    LEVEL_MAZE // лабиринт
} GameMode;

typedef struct
{
    int x;
    int y;
} Segment;

typedef enum
{ // типы еды
    FOOD_NORMAL,
    FOOD_SPEED,
    FOOD_SLOW,
    FOOD_BONUS
} FoodType;

typedef struct
{
    int x;
    int y;
    FoodType type;
} Food;

// typedef struct
// {
//     int x;
//     int y;
//     int active;
// } GrowSegment;

// GrowSegment pendingGrow;

GameMode mode = LEVEL_WRAP;
// GameMode mode = LEVEL_WALLS;

Direction dir = RIGHT;

int gameOver = 0;

int canChangeDirection = 1;

double speed = START_SPEED; // раз в 0.2 секунды

Food food;

int foodScore[] = { 10, 10, 10, 30 };
int foodGrow[] = { 1, 1, 1, 3 };
float foodSpeed[] = { 0.0f, -0.02f, +0.02f, 0.0f };

float foodColor[4][3] =
{
    {1.0f, 0.0f, 0.0f}, // NORMAL - red
    {0.0f, 1.0f, 1.0f}, // SPEED - cyan
    {0.0f, 0.0f, 1.0f}, // SLOW - blue
    {1.0f, 1.0f, 0.0f}  // BONUS - yellow
};

int score = 0;
int growPending = 0;

Segment snake[MAX_LENGTH];
int snakeLength = 3; // начальная длина

double lastTime = 0;

void drawSquare(float x, float y, float width, float height)
{
    glBegin(GL_QUADS);

    glVertex2f(x, y);
    glVertex2f(x + width, y);
    glVertex2f(x + width, y + height);
    glVertex2f(x, y + height);

    glEnd();
}

// рисование текста
void drawText(float x, float y, const char* text, float scale)
{
    char buffer[99999]; // записываем готовые квадратики букв

    // num_quads - количество квадратов, из которых составляются буквы
    int num_quads = stb_easy_font_print(
        0, // x
        0, // y
        (char*)text, // текст
        NULL, // цвет
        buffer, // куда сохранять координаты точек
        sizeof(buffer)
    );

    // Для изменения размера текста
    glPushMatrix(); // сохраняет текущую матрицу трансформаций (позицию/масштаб/поворот)
    glTranslatef(x, y, 0); // переноси текст в точку (x, y)
    glScalef(scale, scale, 1); // маштабируем текст

    glColor3f(1, 1, 1);

    glEnableClientState(GL_VERTEX_ARRAY); // рисование через массив вершин
    glVertexPointer(2, GL_FLOAT, 16, buffer); // указание, где лежат вершины. 2 - задаём координаты двумя числами (x, y) 

    glDrawArrays(GL_QUADS, 0, num_quads * 4); // num_quads * 4 - количество вершин, которые нужно отрисовать

    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();
}

const char* getModeName()
{
    if (mode == LEVEL_WRAP) {
        return "WRAP";
    }

    if (mode == LEVEL_WALLS) {
        return "WALLS";
    }

    return "MAZE";
}

// через стенки (телепорт)
void handleBoundsWrap()
{
    if (snake[0].x < 0)
        snake[0].x = WIDTH - 1;
    if (snake[0].x >= WIDTH)
        snake[0].x = 0;

    if (snake[0].y < 0)
        snake[0].y = HEIGHT - 1;
    if (snake[0].y >= HEIGHT)
        snake[0].y = 0;
}

int isWall(int x, int y)
{

    if (mode == LEVEL_WRAP) {
        return 0;
    }

    // обычные границы
    if (x == 0 || x == WIDTH - 1 || y == 0 || y == HEIGHT - 1)
    {
        return 1;
    }

    if (mode == LEVEL_MAZE)
    {
        int left_b = 6;
        int right_b = 18;
        int top_b = 6;
        int bottom_b = 17;

        int left_s = left_b + 3;
        int right_s = right_b - 3;
        int top_s = top_b + 3;
        int bottom_s = bottom_b;

        // внешние стены
        if (x == 0 || x == WIDTH - 1 || y == 0 || y == HEIGHT - 1) {
            return 1;
        }

        // левая палочка
        if (x == left_b && y >= top_b && y <= bottom_b) {
            return 1;
        }
        if (x == left_s && y >= top_s && y <= bottom_s) {
            return 1;
        }

        // правая палочка
        if (x == right_b && y >= top_b && y <= bottom_b) {
            return 1;
        }
        if (x == right_s && y >= top_s && y <= bottom_s) {
            return 1;
        }

        // верхняя перекладина
        if (y == top_b && x >= left_b && x <= right_b) {
            return 1;
        }
        if (y == top_s && x >= left_s && x <= right_s) {
            return 1;
        }
    }
    return 0;
}

void initSnake()
{
    int ok = 0;

    while (!ok)
    {
        int x = SAFE_MARGIN + rand() % (WIDTH - 2 * SAFE_MARGIN);
        int y = SAFE_MARGIN + rand() % (HEIGHT - 2 * SAFE_MARGIN);

        ok = 1;

        // проверяем тело змейки: голова + 2 сегмента хвоста
        for (int i = 0; i < 3; i++)
        {
            int xi = x - i;
            int yi = y;

            if (xi < 0 || xi >= WIDTH || yi < 0 || yi >= HEIGHT || isWall(xi, yi))
            {
                ok = 0;
                break;
            }
        }

        if (!ok)
            continue;

        // проверяем свободное место перед головой,
        // чтобы змейка не умерла сразу после спавна
        for (int i = 1; i <= SAFE_MARGIN; i++)
        {
            int xi = x + i;
            int yi = y;

            if (xi < 0 || xi >= WIDTH || yi < 0 || yi >= HEIGHT || isWall(xi, yi))
            {
                ok = 0;
                break;
            }
        }

        if (!ok)
            continue;

        snake[0].x = x;
        snake[0].y = y;

        snake[1].x = x - 1;
        snake[1].y = y;

        snake[2].x = x - 2;
        snake[2].y = y;
    }
}

int isInsideField(int x, int y) // внутри поля
{
    return x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT;
}

int isSnakeCell(int x, int y) // внутри змейки
{
    for (int i = 0; i < snakeLength; i++)
    {
        if (snake[i].x == x && snake[i].y == y) {
            return 1;
        }
    }

    return 0;
}

int isFreeCell(int x, int y)
{
    return isInsideField(x, y) && !isWall(x, y) && !isSnakeCell(x, y);
}

void spawnFood()
{
    int ok = 0;
    int attempts = 0;

    while (!ok && attempts < 1000)
    {
        attempts++;

        food.x = rand() % WIDTH;
        food.y = rand() % HEIGHT;

        if (isFreeCell(food.x, food.y))
            ok = 1;
    }

    // если свободное место не найдено
    if (!ok)
    {
        gameOver = 1;
        return;
    }

    int r = rand() % 100;

    if (r < 50)
        food.type = FOOD_NORMAL;
    else if (r < 65)
        food.type = FOOD_BONUS;
    else if (r < 80)
        food.type = FOOD_SLOW;
    else
        food.type = FOOD_SPEED;
}


void checkSelfCollision() // проверка на столкновении головы с телом
{
    for (int i = 1; i < snakeLength; i++)
    {
        if (snake[0].x == snake[i].x &&
            snake[0].y == snake[i].y)
        {
            gameOver = 1;
            printf("Game Over (self collision)\n");
            break;
        }
    }
}

void update()
{
    canChangeDirection = 1;

    // сдвигаем тело
    for (int i = snakeLength - 1; i > 0; i--)
    {
        snake[i] = snake[i - 1];
    }

    // двигаем голову
    if (dir == RIGHT)
        snake[0].x++;
    if (dir == LEFT)
        snake[0].x--;
    if (dir == UP)
        snake[0].y--;
    if (dir == DOWN)
        snake[0].y++;

    if (mode == LEVEL_WRAP)
    {
        if (snake[0].x < 0)
            snake[0].x = WIDTH - 1;
        if (snake[0].x >= WIDTH)
            snake[0].x = 0;
        if (snake[0].y < 0)
            snake[0].y = HEIGHT - 1;
        if (snake[0].y >= HEIGHT)
            snake[0].y = 0;
    }

    // проверка стен
    if (mode != LEVEL_WRAP && isWall(snake[0].x, snake[0].y))
    {
        gameOver = 1;
        return;
    }

    checkSelfCollision();
    // рост
    if (growPending > 0 && snakeLength < MAX_LENGTH)
    {
        snake[snakeLength] = snake[snakeLength - 1];
        snakeLength++;
        growPending--;
    }
    if (snake[0].x == food.x && snake[0].y == food.y)
    {
        int t = food.type;

        score += foodScore[t];
        growPending += foodGrow[t];

        speed += foodSpeed[t];

        // ограничение скорости (чтобы не сломалось)
        if (speed < 0.05)
            speed = 0.05;
        if (speed > 0.5)
            speed = 0.5;

        spawnFood();
    }
}

void restartGame()
{
    snakeLength = 3;
    score = 0;
    gameOver = 0;
    dir = RIGHT;
    growPending = 0;
    speed = START_SPEED;
    canChangeDirection = 1;

    initSnake();

    lastTime = glfwGetTime();

    spawnFood();
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_R)
        {
            restartGame();
            return;
        }

        if (key == GLFW_KEY_M)
        {
            if (mode == LEVEL_WRAP)
                mode = LEVEL_WALLS;
            else if (mode == LEVEL_WALLS)
            {
                mode = LEVEL_MAZE;
            }
            else
            {
                mode = LEVEL_WRAP;
            }
            restartGame();
            return;
        }

        if (canChangeDirection)
        {
            if (key == GLFW_KEY_W && dir != DOWN)
            {
                dir = UP;
                canChangeDirection = 0;
            }

            if (key == GLFW_KEY_S && dir != UP)
            {
                dir = DOWN;
                canChangeDirection = 0;
            }

            if (key == GLFW_KEY_A && dir != RIGHT)
            {
                dir = LEFT;
                canChangeDirection = 0;
            }

            if (key == GLFW_KEY_D && dir != LEFT)
            {
                dir = RIGHT;
                canChangeDirection = 0;
            }
        }
    }
}

void drawSnake()
{

    for (int i = 0; i < snakeLength; i++)
    {
        if (i == 0)
        {
            // Голова
            glColor3f(0.0f, 0.7f, 0.0f);
        }
        else
        {
            // Тело
            glColor3f(0.0f, 0.5f, 0.0f);
        }

        float x = snake[i].x * CELL_SIZE;
        float y = snake[i].y * CELL_SIZE;

        drawSquare(x, y, CELL_SIZE, CELL_SIZE);

        // Глаза только для головы
        if (i == 0)
        {
            glColor3f(0.0f, 0.0f, 0.0f); // чёрные глаза

            float eyeSize = CELL_SIZE / 6.0f;

            // левый глаз
            drawSquare(x + CELL_SIZE * 0.25f, y + CELL_SIZE * 0.25f, eyeSize, eyeSize);

            // правый глаз
            drawSquare(x + CELL_SIZE * 0.65f, y + CELL_SIZE * 0.25f, eyeSize, eyeSize);
        }
    }
}

void drawFood()
{
    int t = food.type;

    glColor3f(
        foodColor[t][0],
        foodColor[t][1],
        foodColor[t][2]);

    drawSquare(food.x * CELL_SIZE, food.y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
}

void drawGrid()
{
    glColor3f(0.12f, 0.12f, 0.12f);

    glBegin(GL_LINES); // рисуем линии

    // вертикальные линии
    for (int x = 0; x <= WIDTH; x++)
    {
        glVertex2f(x * CELL_SIZE, 0);
        glVertex2f(x * CELL_SIZE, WINDOW_HEIGHT);
    }

    // горизонтальные линии
    for (int y = 0; y <= HEIGHT; y++)
    {
        glVertex2f(0, y * CELL_SIZE);
        glVertex2f(WINDOW_WIDTH, y * CELL_SIZE);
    }

    glEnd();
}


void drawWalls()
{
    if (mode == LEVEL_WALLS || mode == LEVEL_MAZE)
    {
        glColor3f(1, 1, 1);

        for (int x = 0; x < WIDTH; x++)
        {
            for (int y = 0; y < HEIGHT; y++)
            {
                if (isWall(x, y))
                {
                    drawSquare(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE);
                }
            }
        }
    }
}

void drawSidebar()
{
    float startX = WIDTH * CELL_SIZE;

    // фон панели
    glColor3f(0.08f, 0.08f, 0.08f);

    drawSquare(startX, 0, SIDEBAR_WIDTH, WINDOW_HEIGHT);

    char text[128];

    // заголовок
    drawText(startX + 20, 40, "SNAKE GAME", 2.0f);

    // очки
    sprintf(text, "Score: %d", score);
    drawText(startX + 20, 100, text, 1.5f);

    // длина
    sprintf(text, "Length: %d", snakeLength);
    drawText(startX + 20, 140, text, 1.5f);

    // режим
    sprintf(text, "Mode: %s", getModeName());
    drawText(startX + 20, 180, text, 1.5f);

    // скорость
    int speedLevel = (int)round((START_SPEED - speed) / 0.02) + 10;

    sprintf(text, "Speed: %d", speedLevel);
    drawText(startX + 20, 220, text, 1.5f);

    // управление
    drawText(startX + 20, 320, "Controls:", 1.5f);
    drawText(startX + 20, 360, "WASD - Move", 1.5f);
    drawText(startX + 20, 400, "R - Restart", 1.5f);
    drawText(startX + 20, 440, "M - Change Mode", 1.5f);
}

void drawGameOver()
{
    // фон
    glColor3f(0.4f, 0.0f, 0.0f);

    glBegin(GL_QUADS);

    glVertex2f(0, 0);
    glVertex2f(WIDTH * CELL_SIZE, 0);
    glVertex2f(WIDTH * CELL_SIZE, WINDOW_HEIGHT);
    glVertex2f(0, WINDOW_HEIGHT);

    glEnd();
}

int main(void)
{
    // Инициализация GLFW
    if (!glfwInit())
    {
        printf("Failed to initialize GLFW\n");
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    // Создание окна
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Snake Game", NULL, NULL);
    if (!window)
    {
        printf("Failed to create window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window); // рисовать именно в окне "windows"
    glfwSetKeyCallback(window, key_callback); // обработка нажатия клавиш
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    // 2D координаты
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity(); // обнуляет текущую матрицу
    glOrtho(0, WINDOW_WIDTH, WINDOW_HEIGHT, 0, -1, 1); // координаты как в пикселях

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    srand(time(NULL));
    initSnake();
    spawnFood();


    while (!glfwWindowShouldClose(window))
    {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT); // очистка экрана
        double currentTime = glfwGetTime(); // текущее время

        if (gameOver)
        {
            glfwSetWindowTitle(window, "GAME OVER");
        }
        else
        {
            glfwSetWindowTitle(window, "Snake");
        }


        if (!gameOver && currentTime - lastTime > speed)
        {
            update();
            lastTime = currentTime;
        }

        if (gameOver)
        {
            drawGameOver();
            drawSidebar();
        }
        else
        {
            drawGrid();
            drawSnake();
            drawFood();
            drawWalls();
            drawSidebar();
        }

        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        {
            if (snakeLength < MAX_LENGTH)
            {
                snakeLength++;
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}