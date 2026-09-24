# Аудит функций imgui_draw.cpp и матрица хуков

## Архитектурные требования к хукам
1. **Только `imgui_draw.cpp`:** остальные файлы ImGui (`imgui.h`, `imgui.cpp`, `imgui_widgets.cpp` и др.) не модифицируются.
2. **Нулевой оверхед по умолчанию:** каждый макрос `#ifndef IMGUI_DRAWLIST_HOOK_...` по умолчанию определяется как `(false)`. Без нашего `IMGUI_USER_CONFIG` поведение и бинарный код идентичны upstream.
3. **Возвращаемое значение `bool`:**
   - `true` — вызов перехвачен рекордером; функция `ImDrawList` немедленно завершает работу (`return;`), минуя стоковую триангуляцию.
   - `false` — вызов не перехвачен; выполняется стандартная генерация вершин/индексов, которая попадает в fallback-меш.

---

## Таблица аудита функций

| № | Функция в `imgui_draw.cpp` | Категория | Статус перехвата | Имя макроса-хука |
|---|---------------------------|-----------|------------------|-------------------|
| 1 | `AddRectFilled` | Высокоуровневый вектор | Обязательно для demo | `IMGUI_DRAWLIST_HOOK_RECT_FILLED` |
| 2 | `AddRect` | Высокоуровневый вектор | Обязательно для demo | `IMGUI_DRAWLIST_HOOK_RECT` |
| 3 | `AddLine` | Высокоуровневый вектор | Обязательно для demo | `IMGUI_DRAWLIST_HOOK_LINE` |
| 4 | `AddLineH` / `AddLineV` | Высокоуровневый вектор | Обязательно для demo | `IMGUI_DRAWLIST_HOOK_LINE_HV` |
| 5 | `AddCircle` | Высокоуровневый вектор | Обязательно для demo | `IMGUI_DRAWLIST_HOOK_CIRCLE` |
| 6 | `AddCircleFilled` | Высокоуровневый вектор | Обязательно для demo | `IMGUI_DRAWLIST_HOOK_CIRCLE_FILLED` |
| 7 | `AddNgon` | Высокоуровневый вектор | Вектор | `IMGUI_DRAWLIST_HOOK_NGON` |
| 8 | `AddNgonFilled` | Высокоуровневый вектор | Вектор | `IMGUI_DRAWLIST_HOOK_NGON_FILLED` |
| 9 | `AddEllipse` | Высокоуровневый вектор | Вектор | `IMGUI_DRAWLIST_HOOK_ELLIPSE` |
| 10 | `AddEllipseFilled` | Высокоуровневый вектор | Вектор | `IMGUI_DRAWLIST_HOOK_ELLIPSE_FILLED` |
| 11 | `AddTriangle` | Высокоуровневый вектор | Вектор | `IMGUI_DRAWLIST_HOOK_TRIANGLE` |
| 12 | `AddTriangleFilled` | Высокоуровневый вектор | Обязательно для demo (стрелки, маркеры) | `IMGUI_DRAWLIST_HOOK_TRIANGLE_FILLED` |
| 13 | `AddQuad` | Высокоуровневый вектор | Вектор | `IMGUI_DRAWLIST_HOOK_QUAD` |
| 14 | `AddQuadFilled` | Высокоуровневый вектор | Вектор | `IMGUI_DRAWLIST_HOOK_QUAD_FILLED` |
| 15 | `AddBezierCubic` | Высокоуровневый вектор | Вектор | `IMGUI_DRAWLIST_HOOK_BEZIER_CUBIC` |
| 16 | `AddBezierQuadratic` | Высокоуровневый вектор | Вектор | `IMGUI_DRAWLIST_HOOK_BEZIER_QUADRATIC` |
| 17 | `AddPolyline` | Высокоуровневый вектор | Обязательно для demo | `IMGUI_DRAWLIST_HOOK_POLYLINE` |
| 18 | `AddConvexPolyFilled` | Высокоуровневый вектор | Обязательно для demo | `IMGUI_DRAWLIST_HOOK_CONVEX_POLY_FILLED` |
| 19 | `AddConcavePolyFilled` | Высокоуровневый вектор | Вектор / Fallback | `IMGUI_DRAWLIST_HOOK_CONCAVE_POLY_FILLED` |
| 20 | `AddText` (с `ImFont*`) | Текст | Обязательно для demo | `IMGUI_DRAWLIST_HOOK_TEXT` |
| 21 | `AddImage` | Растр / Текстура | Обязательно для demo | `IMGUI_DRAWLIST_HOOK_IMAGE` |
| 22 | `AddImageQuad` | Растр / Текстура | Вектор | `IMGUI_DRAWLIST_HOOK_IMAGE_QUAD` |
| 23 | `AddImageRounded` | Растр / Текстура | Вектор | `IMGUI_DRAWLIST_HOOK_IMAGE_ROUNDED` |
| 24 | `AddRectFilledMultiColor` | Градиент | Вектор (лин. градиент) / Fallback | `IMGUI_DRAWLIST_HOOK_RECT_FILLED_MULTICOLOR` |
| 25 | `PushClipRect` | Состояние клиппинга | Обязательно | `IMGUI_DRAWLIST_HOOK_PUSH_CLIP_RECT` |
| 26 | `PopClipRect` | Состояние клиппинга | Обязательно | `IMGUI_DRAWLIST_HOOK_POP_CLIP_RECT` |
| 27 | `PushTexture` | Состояние текстур | Обязательно | `IMGUI_DRAWLIST_HOOK_PUSH_TEXTURE` |
| 28 | `PopTexture` | Состояние текстур | Обязательно | `IMGUI_DRAWLIST_HOOK_POP_TEXTURE` |
| 29 | `AddCallback` | Пользовательский вызов | Обязательно | `IMGUI_DRAWLIST_HOOK_CALLBACK` |
| 30 | `_ResetForNewFrame` | Жизненный цикл | Обязательно | `IMGUI_DRAWLIST_HOOK_RESET` |
| 31 | `Splitter::Split` | Каналы рендеринга | Обязательно (таблицы, колонки) | `IMGUI_SPLITTER_HOOK_SPLIT` |
| 32 | `Splitter::SetCurrentChannel` | Каналы рендеринга | Обязательно (таблицы, колонки) | `IMGUI_SPLITTER_HOOK_SET_CHANNEL` |
| 33 | `Splitter::Merge` | Каналы рендеринга | Обязательно (таблицы, колонки) | `IMGUI_SPLITTER_HOOK_MERGE` |
| 34 | `PrimReserve` / `PrimWrite*` | Низкоуровневая запись вершин | **Fallback-меш** | Захват в команду `Mesh` при неперехваченном коде |
