#ifndef GENERIC_H
#define GENERIC_H 1

// TODO: make draw_light take the parameters of the light (colour, radius,
// position) and build up the lighting struct to pass to fov_circle on its
// ownsome?
void draw_light(map_t *map, fov_settings_type *settings, light_t *l) {
	// don't bother calculating if the light's completely outside the screen.
	if (((l->x + l->radius) >> 12) < map->scrollX ||
	    ((l->x - l->radius) >> 12) > map->scrollX + 32 ||
	    ((l->y + l->radius) >> 12) < map->scrollY ||
	    ((l->y - l->radius) >> 12) > map->scrollY + 24) return;

	// calculate lighting values
	fov_circle(settings, (void*)map, (void*)l,
			l->x>>12, l->y>>12, (l->radius>>12) + 1);

	// since fov_circle doesn't touch the origin tile, we'll do its lighting
	// manually here.
	cell_t *cell = cell_at(map, l->x>>12, l->y>>12);
	if (cell->visible) {
		cell->light += (1<<12);
		cache_t *cache = cache_at(map, l->x>>12, l->y>>12);
		cache->lr = l->r;
		cache->lg = l->g;
		cache->lb = l->b;
		cell->recall = 1<<12;
	}
}

#endif /* GENERIC_H */
