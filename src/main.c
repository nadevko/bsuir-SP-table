#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static SDL_Renderer *g_renderer = nullptr;
static SDL_Window *g_window = nullptr;
static bool g_window_fullscreened = true;

static int ROWS = 10;
static int COLS = 10;
constexpr float LINE_WIDTH = 1.0f;

constexpr SDL_Color BACKGROUND_COLOR = {240, 240, 240, 255}; // light gray
constexpr SDL_FColor GRID_COLOR = {100.0f / 255.0f, 100.0f / 255.0f,
                                   100.0f / 255.0f, 1.0f}; // dark gray

#define SDL_CHECK(call, msg, ret)                                              \
  if (!(call)) {                                                               \
    fprintf(stderr, "%s: %s\n", msg, SDL_GetError());                          \
    ret;                                                                       \
  }

static void cleanup(void) {
  if (g_renderer != nullptr) {
    SDL_DestroyRenderer(g_renderer);
    g_renderer = nullptr;
  }
  if (g_window != nullptr) {
    SDL_DestroyWindow(g_window);
    g_window = nullptr;
  }
  SDL_Quit();
}

void draw(float w, float h) {
  // Draw background
  SDL_SetRenderDrawColor(g_renderer, BACKGROUND_COLOR.r, BACKGROUND_COLOR.g,
                         BACKGROUND_COLOR.b, BACKGROUND_COLOR.a);
  SDL_RenderClear(g_renderer);

  // Total number of lines: (COLS + 1) vertical + (ROWS + 1) horizontal
  int total_lines = (COLS + 1) + (ROWS + 1);
  int vertex_count = total_lines * 4; // 4 vertices per line (quad)
  int index_count = total_lines * 6;  // 6 indices per quad (2 triangles)

  // Allocate vertex and index arrays
  SDL_Vertex *vertices =
      (SDL_Vertex *)malloc(vertex_count * sizeof(SDL_Vertex));
  int *indices = (int *)malloc(index_count * sizeof(int));
  if (!vertices || !indices) {
    fprintf(stderr, "Failed to allocate memory for vertices/indices\n");
    return;
  }

  int v_idx = 0; // Vertex index
  int i_idx = 0; // Index index

  // Vertical lines
  for (int i = 0; i <= COLS; ++i) {
    float x = (float)i * w / COLS;
    // Quad for vertical line: (x - LINE_WIDTH/2, 0) to (x + LINE_WIDTH/2, h)
    vertices[v_idx + 0] =
        (SDL_Vertex){{x - LINE_WIDTH / 2, 0}, GRID_COLOR, {0, 0}}; // Top-left
    vertices[v_idx + 1] =
        (SDL_Vertex){{x + LINE_WIDTH / 2, 0}, GRID_COLOR, {1, 0}}; // Top-right
    vertices[v_idx + 2] = (SDL_Vertex){
        {x - LINE_WIDTH / 2, h}, GRID_COLOR, {0, 1}}; // Bottom-left
    vertices[v_idx + 3] = (SDL_Vertex){
        {x + LINE_WIDTH / 2, h}, GRID_COLOR, {1, 1}}; // Bottom-right

    // Indices for two triangles: {0, 1, 2} and {2, 1, 3}
    indices[i_idx + 0] = v_idx + 0;
    indices[i_idx + 1] = v_idx + 1;
    indices[i_idx + 2] = v_idx + 2;
    indices[i_idx + 3] = v_idx + 2;
    indices[i_idx + 4] = v_idx + 1;
    indices[i_idx + 5] = v_idx + 3;

    v_idx += 4;
    i_idx += 6;
  }

  // Horizontal lines
  for (int i = 0; i <= ROWS; ++i) {
    float y = (float)i * h / ROWS;
    // Quad for horizontal line: (0, y - LINE_WIDTH/2) to (w, y + LINE_WIDTH/2)
    vertices[v_idx + 0] =
        (SDL_Vertex){{0, y - LINE_WIDTH / 2}, GRID_COLOR, {0, 0}}; // Top-left
    vertices[v_idx + 1] =
        (SDL_Vertex){{w, y - LINE_WIDTH / 2}, GRID_COLOR, {1, 0}}; // Top-right
    vertices[v_idx + 2] = (SDL_Vertex){
        {0, y + LINE_WIDTH / 2}, GRID_COLOR, {0, 1}}; // Bottom-left
    vertices[v_idx + 3] = (SDL_Vertex){
        {w, y + LINE_WIDTH / 2}, GRID_COLOR, {1, 1}}; // Bottom-right

    // Indices for two triangles
    indices[i_idx + 0] = v_idx + 0;
    indices[i_idx + 1] = v_idx + 1;
    indices[i_idx + 2] = v_idx + 2;
    indices[i_idx + 3] = v_idx + 2;
    indices[i_idx + 4] = v_idx + 1;
    indices[i_idx + 5] = v_idx + 3;

    v_idx += 4;
    i_idx += 6;
  }

  // Render all lines in one call
  SDL_RenderGeometry(g_renderer, NULL, vertices, vertex_count, indices,
                     index_count);

  // Clean up
  free(vertices);
  free(indices);

  SDL_RenderPresent(g_renderer);
}

int main(int argc, char *argv[]) {
  if (argc == 3) {
    ROWS = atoi(argv[1]);
    COLS = atoi(argv[2]);
    if (ROWS <= 0 || COLS <= 0) {
      fprintf(stderr, "Columns and rows must be positive integers.\n");
      return 1;
    }
  } else if (argc != 1) {
    fprintf(stderr, "Usage: %s [rows] [columns]\n", argv[0]);
    return 1;
  }

  SDL_CHECK(SDL_Init(SDL_INIT_VIDEO), "SDL initialisation failed", return 1);
  atexit(cleanup);

  SDL_CHECK(SDL_CreateWindowAndRenderer("Grid Example", 300, 480,
                                        SDL_WINDOW_RESIZABLE, &g_window,
                                        &g_renderer),
            "Window and renderer creation failed", return 1);

  SDL_CHECK(SDL_SetWindowFullscreen(g_window, g_window_fullscreened),
            "Failed to set fullscreen mode", );

  bool running = true;
  SDL_Event event;

  int w, h;
  SDL_GetWindowSize(g_window, &w, &h);
  draw(w, h);

  while (running) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_EVENT_QUIT:
        running = false;
        break;
      case SDL_EVENT_KEY_DOWN:
        switch (event.key.key) {
        case SDLK_ESCAPE:
          running = false;
          break;
        }
        break;
      case SDL_EVENT_WINDOW_RESIZED:
        SDL_GetWindowSize(g_window, &w, &h);
        draw(w, h);
        break;
      }
    }
  }

  return 0;
}