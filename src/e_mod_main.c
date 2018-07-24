#include "e_mod_main.h"

#define WINDOW_HEIGHT   380
#define TEXT_DIST       45
#define SIDE_OFFSET     30


static int            _ngi_win_free(Ngi_Win *win);

static Eina_Bool      _ngi_cb_container_resize(void *data, int type, void *event);

static Eina_Bool      _ngi_win_cb_mouse_in(void *data, int type, void *event);
static Eina_Bool      _ngi_win_cb_mouse_out(void *data, int type, void *event);
static Eina_Bool      _ngi_win_cb_mouse_down(void *data, int type, void *event);
static Eina_Bool      _ngi_win_cb_mouse_up(void *data, int type, void *event);
static Eina_Bool      _ngi_win_cb_mouse_move(void *data, int type, void *event);
static Eina_Bool      _ngi_win_cb_mouse_wheel(void *data, int type, void *event);

static Eina_Bool      _ngi_win_cb_desk_show(void *data, int type, void *event);
static Eina_Bool      _ngi_win_cb_border_event(void *data, int type, void *event);

static void           _ngi_zoom_in(Ng *ng);
static void           _ngi_zoom_out(Ng *ng);
static void           _ngi_item_appear(Ng *ng, Ngi_Item *it);
static void           _ngi_item_disappear(Ng *ng, Ngi_Item *it);
static double         _ngi_zoom_function(Ng *ng, double to, double pos);
static Eina_Bool      _ngi_animator(void *data);
static void           _ngi_redraw(Ng *ng);
static int            _ngi_autohide(Ng *ng, int hide);
static Eina_Bool      _ngi_win_border_intersects(Ng *ng);
static void           _ngi_label_pos_set(Ng *ng);

static Ngi_Win *     _ngi_composite_win_new(Ng *ng);
static Ngi_Win *     _ngi_desktop_win_new(Ng *ng);
static Ngi_Win *     _ngi_shaped_win_new(Ng *ng);

static void          _ngi_win_stacking_auto_adjust(Ng *ng);
static void          _ngi_win_stacking_set(Ng *ng);
static Eina_Bool     _ngi_win_shaped_pointer_update(void *data);
static Eina_Bool     _ngi_win_cb_change_event(void *data, int type, void *event);

static int initialized = 0;
static int mouse_win_x = 0;
static int mouse_win_y = 0;

static int ENGAGE_EVENT_WIN_CHANGE = 0;
int engage_log;
E_Config_DD *ngi_conf_edd = NULL;
E_Config_DD *ngi_conf_item_edd = NULL;
E_Config_DD *ngi_conf_box_edd = NULL;
E_Config_DD *ngi_conf_gadcon_edd = NULL;

Config *ngi_config = NULL;

E_Int_Menu_Augmentation *maug = NULL;


static Eina_Bool
_ngi_win_cb_composite_activated(void *data EINA_UNUSED, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ng *ng = data;
   if (!ng) return EINA_TRUE;


   if (ng->cfg->shaped_set || (ng->win_type != ENGAGE_WIN_COMPOSITE))
     {
        ng->cfg->shaped_set = EINA_FALSE;
        ngi_config->use_composite = EINA_TRUE;

        evas_object_hide(ng->o_label);
        Eina_List *l;
        Config_Item *ci;

        INF("Composite activated");
        EINA_LIST_FOREACH(ngi_config->items, l, ci)
          {
             evas_object_hide(ci->ng->o_label);
             ngi_free(ci->ng);
             ngi_new(ci);
          }
     }
   return EINA_TRUE;
}

static Eina_Bool
_ngi_win_cb_composite_deactivated(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ng *ng = data;
   if (!ng) return EINA_FALSE;


   if (ng->win_type != ENGAGE_WIN_DESKTOP)
     {
        ng->cfg->shaped_set = EINA_FALSE;
        ngi_config->use_composite = EINA_FALSE;

        evas_object_hide(ng->o_label);
        Eina_List *l;
        Config_Item *ci;

        INF("Composite deactivated");
        EINA_LIST_FOREACH(ngi_config->items, l, ci)
          {
             evas_object_hide(ci->ng->o_label);
             ngi_free(ci->ng);
             ngi_new(ci);
          }
     }


   return EINA_TRUE;
}

static Eina_Bool
_ngi_win_cb_change_event(void *data, int type EINA_UNUSED, void *event EINA_UNUSED)
{
   Ng *ng = data;
   if (!ng) return EINA_TRUE;


   if (ng->cfg->shaped_set && !ng->shaped_active)
     {
        evas_object_hide(ng->o_label);
        Eina_List *l;
        Config_Item *ci;

        //WRN("ENGAGE_EVENT_WIN_CHANGE Shaped: %d", ng->cfg->shaped_set);
        EINA_LIST_FOREACH(ngi_config->items, l, ci)
          {
             evas_object_hide(ci->ng->o_label);
             ngi_free(ci->ng);
             ngi_new(ci);
             _ngi_win_shaped_pointer_update(ci->ng);
             ci->ng->shaped_pointer =
                ecore_timer_add(0.015, _ngi_win_shaped_pointer_update, ci->ng);
          }
     }
   else if (!ng->cfg->shaped_set && ng->shaped_active)
     {
        Eina_List *l;
        Config_Item *ci;

        //WRN("ENGAGE_EVENT_WIN_CHANGE Shaped: %d", ng->cfg->shaped_set);
        EINA_LIST_FOREACH(ngi_config->items, l, ci)
          {
             ngi_free(ci->ng);
             ngi_new(ci);
          }
     }
   return EINA_TRUE;
}

static Eina_Bool
_ngi_win_shaped_pointer_update(void *data)
{
   Ecore_Event_Mouse_Move ev_move;
   int root_x, root_y;
   int win_x, win_y;
   Ng *ng;

   ng = data;
   ecore_x_pointer_root_xy_get(&root_x, &root_y);


   if (ng->cfg->orient == E_GADCON_ORIENT_BOTTOM)
     {
        win_x = root_x - ((ng->zone->w /2) - (ng->win->rect.width /2));
        win_y = (root_y - ng->zone->h) + ng->win->rect.height;

        if (((win_x <= 1) || (win_x >= (int)ng->win->rect.width)) ||
            ((win_y <= 1) || (win_y >= (int)ng->win->rect.height)))
          {
             ng->hide = EINA_TRUE;
             ngi_mouse_out(ng);
          }
     }
   else if (ng->cfg->orient == E_GADCON_ORIENT_LEFT)
     {
        win_x = root_x;
        win_y = root_y - ((ng->zone->h /2) - (ng->win->rect.height /2));

        if (((win_x >= (int)ng->win->rect.width) ||
            ((win_y <= 1) || (win_y >= (int)ng->win->rect.height))))
          {
             ng->hide = EINA_TRUE;
             ngi_mouse_out(ng);
          }
     }
   else if (ng->cfg->orient == E_GADCON_ORIENT_RIGHT)
     {
        win_x = ng->zone->w - root_x;
        win_y = root_y - ((ng->zone->h /2) - (ng->win->rect.height /2));

        if (((win_x >= (int)ng->win->rect.width) ||
            ((win_y <= 1) || (win_y >= (int)ng->win->rect.height))))
          {
             ng->hide = EINA_TRUE;
             ngi_mouse_out(ng);
          }
     }
   else if (ng->cfg->orient == E_GADCON_ORIENT_TOP)
     {
        win_x = root_x - ((ng->zone->w /2) - (ng->win->rect.width /2));
        win_y = root_y;

        if (((win_x <= 1) || (win_x >= (int)ng->win->rect.width)) ||
            (win_y >= (int)ng->win->rect.height))
          {
             ng->hide = EINA_TRUE;
             ngi_mouse_out(ng);
          }
     }

  // INF("Zone: w=%d h=%d :: Win: x=%d y=%d", ng->zone->w, ng->zone->h, win_x, win_y);
   if ((mouse_win_x == win_x) && (mouse_win_y == win_y))
     return ECORE_CALLBACK_RENEW;

   mouse_win_x = win_x;
   mouse_win_y = win_y;

   ev_move.x = win_x;
   ev_move.y = win_y;
   ev_move.root.x = root_x;
   ev_move.root.y = root_y;
   ev_move.event_window = ng->win->input;

   _ngi_win_cb_mouse_move(ng, ECORE_EVENT_MOUSE_MOVE, &ev_move);
   return ECORE_CALLBACK_RENEW;
}

Ng *
ngi_new(Config_Item *cfg)
{
   Ng *ng;
   E_Zone *zone;
   Config_Box *cfg_box;
   Eina_List *l;
   int alpha;

   zone = (E_Zone *)e_util_container_zone_number_get(cfg->container, cfg->zone);
   if (!zone) return NULL;

   ng = E_NEW(Ng, 1);
   cfg->ng = ng;
   ng->cfg = cfg;

   ngi_config->instances = eina_list_append(ngi_config->instances, ng);

   ng->zone = zone;

   //ng->win = _ngi_win_new(ng);

   if (ng->cfg->shaped_set)
     ng->win = _ngi_shaped_win_new(ng);
   else
     ng->win = _ngi_composite_win_new(ng);

   if (!ng->win)
     {
        ng->win = _ngi_desktop_win_new(ng);
     }

   _ngi_win_stacking_auto_adjust(ng);

   ng->zoom = 1.0;
   ng->size = ng->cfg->size;

   ng->state = unzoomed;
   ng->hide_state = show;
   ng->hide = EINA_TRUE;
   
   ng->clip = evas_object_rectangle_add(ng->evas);
   evas_object_color_set(ng->clip, 255, 255, 255, 255);

   ng->bg_clip = evas_object_rectangle_add(ng->evas);
   alpha = cfg->alpha;
   evas_object_color_set(ng->bg_clip, alpha, alpha, alpha, alpha);

   ng->o_bg    = edje_object_add(ng->evas);
   ng->o_frame = edje_object_add(ng->evas);
   ng->o_label = edje_object_add(ng->evas);
   ng->es = e_shelf_zone_dummy_new(zone, ng->o_frame, eina_list_count(ngi_config->instances));
   
   if (ng->es)
     {
        e_shelf_orient(ng->es, cfg->orient);
        ng->es->cfg->autohide = ng->cfg->autohide;
        ng->es->cfg->overlap = (ng->cfg->autohide == AUTOHIDE_OVERLAP);
     }

   switch(cfg->orient)
     {
      case E_GADCON_ORIENT_BOTTOM:
	 ngi_object_theme_set(ng->o_bg,	  "e/modules/engage/bg_bottom");
	 ngi_object_theme_set(ng->o_frame,"e/modules/engage/frame_bottom");
	 ngi_object_theme_set(ng->o_label,"e/modules/engage/label_bottom");
	 break;

      case E_GADCON_ORIENT_TOP:
	 ngi_object_theme_set(ng->o_bg,	  "e/modules/engage/bg_top");
	 ngi_object_theme_set(ng->o_frame,"e/modules/engage/frame_top");
	 ngi_object_theme_set(ng->o_label,"e/modules/engage/label_top");
         break;

      case E_GADCON_ORIENT_LEFT:
	 ngi_object_theme_set(ng->o_bg,	  "e/modules/engage/bg_left");
	 ngi_object_theme_set(ng->o_frame,"e/modules/engage/frame_left");
	 ngi_object_theme_set(ng->o_label,"e/modules/engage/label_left");
         break;

      case E_GADCON_ORIENT_RIGHT:
	 ngi_object_theme_set(ng->o_bg,	  "e/modules/engage/bg_right");
	 ngi_object_theme_set(ng->o_frame,"e/modules/engage/frame_right");
	 ngi_object_theme_set(ng->o_label,"e/modules/engage/label_right");
         break;
     }

   const char *data = edje_object_data_get(ng->o_frame, "clip_separator");
   if (data) ng->opt.clip_separator = atoi(data);

   data = edje_object_data_get(ng->o_frame, "icon_spacing");
   if (data) ng->opt.item_spacing = atoi(data);

   data = edje_object_data_get(ng->o_frame, "edge_offset");
   if (data) ng->opt.edge_offset = atoi(data);

   data = edje_object_data_get(ng->o_frame, "separator_width");
   if (data) ng->opt.separator_width = atoi(data);

   data = edje_object_data_get(ng->o_frame, "bg_offset");
   if (data) ng->opt.bg_offset = atoi(data);

   data = edje_object_data_get(ng->o_frame, "keep_overlay_pos");
   if (data) ng->opt.keep_overlay_pos = atof(data);

   ng->opt.fade_duration = 0.25;

   evas_object_show(ng->o_frame);
   evas_object_show(ng->o_bg);

   evas_object_clip_set(ng->o_bg, ng->bg_clip);

   if (cfg->show_label)
     evas_object_show(ng->o_label);

   if (cfg->show_background)
     evas_object_show(ng->bg_clip);


   ngi_win_position_calc(ng->win);
   if(ng->win->popup)
     e_popup_show(ng->win->popup);

   EINA_LIST_FOREACH (cfg->boxes, l, cfg_box)
   {
      switch (cfg_box->type)
        {
         case launcher:
            ngi_launcher_new(ng, cfg_box);
            break;

         case taskbar:
            ngi_taskbar_new(ng, cfg_box);
            break;

         case gadcon:
            ngi_gadcon_new(ng, cfg_box);
            break;
        }
   }
#define HANDLE(_event, _cb) \
   ng->handlers = eina_list_append(ng->handlers, ecore_event_handler_add(_event, _cb, ng));

   ENGAGE_EVENT_WIN_CHANGE = ecore_event_type_new();
   if (!ng->cfg->shaped_set)
     {
        HANDLE(ECORE_EVENT_MOUSE_MOVE,        _ngi_win_cb_mouse_move);
     }
   HANDLE(ECORE_X_EVENT_MOUSE_OUT,       _ngi_win_cb_mouse_out);
   HANDLE(ECORE_X_EVENT_MOUSE_IN,        _ngi_win_cb_mouse_in);
   HANDLE(ECORE_EVENT_MOUSE_BUTTON_DOWN, _ngi_win_cb_mouse_down);
   HANDLE(ECORE_EVENT_MOUSE_BUTTON_UP,   _ngi_win_cb_mouse_up);
   HANDLE(ECORE_EVENT_MOUSE_WHEEL,       _ngi_win_cb_mouse_wheel);

   HANDLE(E_EVENT_DESK_SHOW,       _ngi_win_cb_desk_show);
   HANDLE(E_EVENT_BORDER_PROPERTY, _ngi_win_cb_border_event);
   HANDLE(E_EVENT_BORDER_MOVE,     _ngi_win_cb_border_event);
   HANDLE(E_EVENT_BORDER_RESIZE,   _ngi_win_cb_border_event);
   HANDLE(E_EVENT_BORDER_ADD,      _ngi_win_cb_border_event);
   HANDLE(E_EVENT_BORDER_REMOVE,   _ngi_win_cb_border_event);

   HANDLE(ENGAGE_EVENT_WIN_CHANGE, _ngi_win_cb_change_event);
   HANDLE(E_EVENT_COMPOSITE_ACTIVE, _ngi_win_cb_composite_activated);
   HANDLE(E_EVENT_COMPOSITE_INACTIVE, _ngi_win_cb_composite_deactivated);
#undef HANDLE


   if (ng->cfg->autohide == AUTOHIDE_FULLSCREEN)
     {
	ng->hide = e_desk_current_get(ng->zone)->fullscreen_borders;
     }
   else if (ng->cfg->autohide == AUTOHIDE_OVERLAP)
     {
	ng->hide = _ngi_win_border_intersects(ng);
     }
   else if (ng->cfg->stacking == ENGAGE_BELOW_FULLSCREEN)
     {
	int fullscreen = e_desk_current_get(ng->zone)->fullscreen_borders;

	if (fullscreen)
	  e_popup_hide(ng->win->popup);
	else
	  e_popup_show(ng->win->popup);
     }

   if(ng->cfg->shaped_set)
     e_popup_show(ng->win->popup);

   if (ng->cfg->autohide && ng->hide)
     {
	ng->hide = EINA_TRUE;
        ng->hide_step = ng->size + ng->opt.edge_offset + ng->opt.bg_offset;
        ng->hide_state = hidden;

        if(ng->cfg->shaped_set)
          {
             ng->hide = EINA_FALSE;
             ng->hide_state = showing;
          }
     }

   ngi_thaw(ng);

   return ng;
}

void
ngi_free(Ng *ng)
{
   Ngi_Box *box;
   Ecore_Event_Handler *h;

   ngi_config->instances = eina_list_remove(ngi_config->instances, ng);

   while(ng->boxes)
     {
        box = ng->boxes->data;
        if (box->cfg->type == taskbar)
           ngi_taskbar_remove(box);
        else if (box->cfg->type == launcher)
           ngi_launcher_remove(box);
        else if (box->cfg->type == gadcon)
           ngi_gadcon_remove(box);
     }

   if (ng->animator)
      ecore_animator_del(ng->animator);

   if (ng->composite_timer)
     ecore_timer_del(ng->composite_timer);

   if (ng->force_timer)
     ecore_timer_del(ng->force_timer);

   if (ng->shaped_pointer)
     ecore_timer_del(ng->shaped_pointer);

   if (ng->menu_wait_timer)
      ecore_timer_del(ng->menu_wait_timer);

   if (ng->mouse_in_timer)
      ecore_timer_del(ng->mouse_in_timer);

   if (ng->effect_timer)
      ecore_timer_del(ng->effect_timer);

   if (ng->o_bg)
      evas_object_del(ng->o_bg);

   if (ng->o_frame)
      evas_object_del(ng->o_frame);

   if (ng->clip)
      evas_object_del(ng->clip);

   if (ng->bg_clip)
     evas_object_del(ng->bg_clip);

   if (ng->win)
      _ngi_win_free(ng->win);

   EINA_LIST_FREE(ng->handlers, h)
   ecore_event_handler_del(h);

   free(ng);
}

Eina_Bool
ngi_object_theme_set(Evas_Object *obj, const char *part)
{
   if (e_theme_edje_object_set(obj, "base/theme/modules/engage", part))
     return 1;
   else
     return edje_object_file_set(obj, ngi_config->theme_path, part);
}

static void
_ngi_win_stacking_set(Ng *ng)
{
   if (!ng) return;
   if (!ng->win) return;

   switch (ng->cfg->stacking)
     {
      case ENGAGE_BELOW_FULLSCREEN:
         //INF("ENGAGE_BELOW_FULLSCREEN");
	 e_container_window_raise(ng->zone->container, ng->win->evas_win, E_LAYER_ABOVE);
         e_container_window_raise(ng->zone->container, ng->win->input, E_LAYER_ABOVE);
	 break;

      case ENGAGE_ON_DESKTOP:
         //INF("ENGAGE_ON_DESKTOP");
         e_container_window_raise(ng->zone->container, ng->win->evas_win, E_LAYER_BELOW);
	 e_container_window_raise(ng->zone->container, ng->win->input, E_LAYER_BELOW);
	 break;

      case ENGAGE_ABOVE_ALL:
         //INF("ENGAGE_ABOVE_ALL");
	 e_container_window_raise(ng->zone->container, ng->win->evas_win, E_LAYER_FULLSCREEN);
	 e_container_window_raise(ng->zone->container, ng->win->input, 999);
     }
}

static void
_ngi_win_stacking_auto_adjust(Ng *ng)
{
   if (!ng) return;

   if (ecore_x_screen_is_composited(e_manager_current_get()->num))
     {
        if ((ng->cfg->stacking != ng->cfg->old_stacking) &&
            (ng->cfg->stacking == ENGAGE_ON_DESKTOP))
          {
             ng->cfg->stacking = ng->cfg->old_stacking;
          }
     }
   else
     {
        if (ng->cfg->stacking != ENGAGE_ON_DESKTOP)
          {
             ng->cfg->old_stacking = ng->cfg->stacking;
             ng->cfg->stacking = ENGAGE_ON_DESKTOP;
          }
     }

   if (ng->cfg->shaped_set)
     ng->cfg->stacking = ENGAGE_ABOVE_ALL;

   _ngi_win_stacking_set(ng);
}

static Ngi_Win *
_ngi_composite_win_new(Ng *ng)
{
   Ngi_Win *win;

   win = E_NEW(Ngi_Win, 1);
   if (!win) return NULL;

   if (!ecore_x_screen_is_composited(e_manager_current_get()->num))
     return NULL;

   win->ng = ng;

   INF("win_type = ENGAGE_WIN_COMPOSITE");
   ng->win_type = ENGAGE_WIN_COMPOSITE;
   win->popup = e_popup_new(ng->zone, 0, 0, 0, 0);
   ecore_evas_alpha_set(win->popup->ecore_evas, 1);
   win->popup->evas_win = ecore_evas_software_x11_window_get(win->popup->ecore_evas);
   win->input = win->popup->evas_win;
   win->drop_win = E_OBJECT(win->popup);
   ng->evas = win->popup->evas;

   e_drop_xdnd_register_set(win->input, 1);
   ecore_x_netwm_window_type_set(win->evas_win, ECORE_X_WINDOW_TYPE_DOCK);

   return win;
}

static Ngi_Win *
_ngi_desktop_win_new(Ng *ng)
{
   Ngi_Win *win;

   win = E_NEW(Ngi_Win, 1);
   if (!win) return NULL;

   if (ecore_x_screen_is_composited(e_manager_current_get()->num))
     return NULL;

   win->ng = ng;

   INF("win_type = ENGAGE_WIN_DESKTOP");
   ng->win_type = ENGAGE_WIN_DESKTOP;
   ng->evas = win->ng->zone->container->bg_evas;

   win->input = ecore_x_window_input_new(ng->zone->container->win, 0, 0, 1, 1);
   ecore_x_window_show(win->input);
   e_drop_xdnd_register_set(win->input, 1);

   win->fake_iwin = E_OBJECT_ALLOC(E_Win, E_WIN_TYPE, NULL);
   win->fake_iwin->x = 0;
   win->fake_iwin->y = 0;
   win->fake_iwin->w = 1;
   win->fake_iwin->h = 1;
   win->fake_iwin->evas_win = win->input;
   win->drop_win = E_OBJECT(win->fake_iwin);

   e_drop_xdnd_register_set(win->input, 1);
   ecore_x_netwm_window_type_set(win->evas_win, ECORE_X_WINDOW_TYPE_DOCK);

   return win;
}


static Ngi_Win *
_ngi_shaped_win_new(Ng *ng)
{
   Ngi_Win *win;

   win = E_NEW(Ngi_Win, 1);
   if (!win) return NULL;

   if (ecore_x_screen_is_composited(e_manager_current_get()->num))
     return NULL;

   win->ng = ng;
   INF("win_type = ENGAGE_WIN_SHAPED");
   ng->win_type = ENGAGE_WIN_SHAPED;
   ng->shaped_active = EINA_TRUE;

   win->popup = e_popup_new(ng->zone, 0, 0, 0, 0);
   ecore_evas_alpha_set(win->popup->ecore_evas, 1);
   win->popup->evas_win = ecore_evas_software_x11_window_get(win->popup->ecore_evas);
   win->input = win->popup->evas_win;
   win->drop_win = E_OBJECT(win->popup);
   ng->evas = win->popup->evas;

   ecore_evas_shaped_set(win->popup->ecore_evas, 1);
   ng->cfg->stacking = ENGAGE_ABOVE_ALL;

   e_drop_xdnd_register_set(win->input, 1);
   ecore_x_netwm_window_type_set(win->evas_win, ECORE_X_WINDOW_TYPE_DOCK);

   return win;
}

static int
_ngi_win_free(Ngi_Win *win)
{
   e_drop_xdnd_register_set(win->input, 0);
   if(win->popup)
     e_object_del(E_OBJECT(win->popup));

   if (win->fake_iwin)
     {
   	ecore_x_window_free(win->input);
   	free(win->fake_iwin);
     }

   free(win);

   return 1;
}

static Eina_Bool
_ngi_cb_container_resize(void *data EINA_UNUSED, int ev_type EINA_UNUSED, void *event_info EINA_UNUSED)
{
   Config_Item *ci;
   E_Zone *zone;
   Eina_List *l;
   Ng *ng;

   if (!initialized)
      return EINA_TRUE;

   EINA_LIST_FOREACH (ngi_config->items, l, ci)
   {
      if (!ci->ng)
        {
           ngi_new(ci);
           continue;
        }

      ng = ci->ng;
      zone = e_util_container_zone_number_get(ci->container, ci->zone);
      if (!zone)
        {
           ngi_free(ng);
           continue;
        }

      ngi_win_position_calc(ng->win);
      ngi_reposition(ng);
      ngi_input_extents_calc(ng);
      ngi_thaw(ng);
   }
   return EINA_TRUE;
}

static void
_ngi_netwm_icon_geometry_set(E_Border *bd, int x, int y, int w, int h)
{
   unsigned int sizes[4];
   sizes[0] = x;
   sizes[1] = y;
   sizes[2] = w;
   sizes[3] = h;
   ecore_x_window_prop_card32_set
     (bd->win, ECORE_X_ATOM_NET_WM_ICON_GEOMETRY, sizes, 4);
}
void
ngi_input_extents_calc(Ng *ng)
{
   int item_zoomed;
   Ngi_Win *win = ng->win;
   Ngi_Box *box;
   Eina_List *l;

   if (ng->mouse_in)
     item_zoomed = ng->size * ng->cfg->zoomfactor + ng->opt.bg_offset + ng->opt.edge_offset;
   else if (ng->hide_state == hidden)
     item_zoomed = 2;
   else
     item_zoomed = ng->size + ng->opt.bg_offset + ng->opt.edge_offset;

   switch (ng->cfg->orient)
     {
      case E_GADCON_ORIENT_BOTTOM:
         win->rect.x = ng->start;
         if(ng->win->popup)
           win->rect.y = ng->win->popup->h - item_zoomed;

         if(ng->win->fake_iwin)
           win->rect.y = ng->win->h - item_zoomed;

         win->rect.width = ng->w;
         win->rect.height = item_zoomed;
         break;

      case E_GADCON_ORIENT_TOP:
         win->rect.x = ng->start;
         win->rect.y = 0;
         win->rect.width = ng->w;
	 win->rect.height = item_zoomed;
         break;

      case E_GADCON_ORIENT_LEFT:
         win->rect.x = 0;
         win->rect.y = ng->start;
         win->rect.width = item_zoomed;
         win->rect.height = ng->w;

         break;

      case E_GADCON_ORIENT_RIGHT:
         if(ng->win->popup)
           win->rect.x = ng->win->popup->w - item_zoomed;

         if(ng->win->fake_iwin)
           win->rect.x = ng->win->w - item_zoomed;

         win->rect.y = ng->start;
         win->rect.width = item_zoomed;
         win->rect.height = ng->w;
         break;
     }

   //e_container_window_raise(ng->zone->container, win->input, 999);//FIXME: not sure if this is still needed

   if ((ngi_config->use_composite || ng->cfg->shaped_set) && (ng->win->popup))
     {
	ecore_x_window_shape_input_rectangles_set(win->input, &win->rect, 1);
     }
   else
     {
        if(ng->win->popup)
          ecore_x_window_move_resize(win->input,
				   ng->zone->x + win->popup->x + win->rect.x,
				   ng->zone->y + win->popup->y + win->rect.y,
				   win->rect.width, win->rect.height);

        if(ng->win->fake_iwin)
          ecore_x_window_move_resize(win->input,
			      ng->zone->x + win->x + win->rect.x,
			      ng->zone->y + win->y + win->rect.y,
			      win->rect.width, win->rect.height);

     }

   EINA_LIST_FOREACH (ng->boxes, l, box)
     {
	int w = box->w;

	if (!box->drop_handler)
	  continue;

	if (w < 10) w = 10;

	switch (ng->cfg->orient)
	  {
	   case E_GADCON_ORIENT_BOTTOM:
              if(ng->win->popup)
                {
                   e_drop_handler_geometry_set
                      (box->drop_handler,
                       ng->zone->x + box->pos,
                       ng->zone->y + win->popup->h - item_zoomed,
                       w, item_zoomed);
                }
              
              if(ng->win->fake_iwin)
                {
                   e_drop_handler_geometry_set
                      (box->drop_handler,
                       ng->zone->x + box->pos,
                       ng->zone->y + win->h - item_zoomed,
                       w, item_zoomed);
                }

	      break;

	   case E_GADCON_ORIENT_TOP:
	      e_drop_handler_geometry_set
		(box->drop_handler,
		 ng->zone->x + box->pos,
		 ng->zone->y,
		 w, item_zoomed);
	      break;

	   case E_GADCON_ORIENT_LEFT:
	      e_drop_handler_geometry_set
		(box->drop_handler,
		 ng->zone->x,
		 ng->zone->y + box->pos,
		 item_zoomed, w);
	      break;

	   case E_GADCON_ORIENT_RIGHT:
              if(ng->win->popup)
                {
                   e_drop_handler_geometry_set
                      (box->drop_handler,
                       ng->zone->x + win->popup->w - item_zoomed,		 
                       ng->zone->y + box->pos,		 
                       item_zoomed, w);
                }
              
              if(ng->win->fake_iwin)
                {
                   e_drop_handler_geometry_set
                      (box->drop_handler,
                       ng->zone->x + win->w - item_zoomed,		 
                       ng->zone->y + box->pos,		 
                       item_zoomed, w);
                }

	      break;
	  }
     }
}

void
ngi_win_position_calc(Ngi_Win *win)
{
   Ng *ng = win->ng;

   E_Gadcon_Orient orient = (E_Gadcon_Orient)ng->cfg->orient;
   int size = WINDOW_HEIGHT;

   //DBG("before_position_calc : x:%d y:%d w:%d h:%d", win->x, win->y, win->w, win->h);

 //  if(win->popup)
 //    DBG("POPUP:brefore_position_calc : x:%d y:%d w:%d h:%d", win->popup->x, win->popup->y, win->popup->w, win->popup->h);

   switch (orient)
     {
      case E_GADCON_ORIENT_LEFT:
         if(win->popup)
            e_popup_move_resize(win->popup, 0, 0, size, ng->zone->h);
         
         if(win->fake_iwin)
           {
              win->w = size;
              win->h = ng->zone->h;
           }
	 ng->horizontal = EINA_FALSE;
	 break;

      case E_GADCON_ORIENT_RIGHT:

         if(win->popup)
            e_popup_move_resize(win->popup, ng->zone->w - size, 0, size, ng->zone->h);
         
         if(win->fake_iwin)
           {
              win->h = ng->zone->h;
              win->w = ng->zone->w;
           }
	 ng->horizontal = EINA_FALSE;
	 break;

      case E_GADCON_ORIENT_TOP:

         if(win->popup)
            e_popup_move_resize(win->popup, 0, 0, ng->zone->w, size);
         
         if(win->fake_iwin)
           {
              win->w = ng->zone->w;
              win->h = size;
           }
	 ng->horizontal = EINA_TRUE;
	 break;

      case E_GADCON_ORIENT_BOTTOM:
         if(win->popup)
            e_popup_move_resize(win->popup, 0, ng->zone->h - size, ng->zone->w, size);
         
         if(win->fake_iwin)
           {
              win->w = ng->zone->w;
              win->h = ng->zone->h;
           }
	 
         ng->horizontal = EINA_TRUE;
	 break;

      default:
	 break;
     }

   /*
   if(win->popup)
     DBG("after_position_calc : x:%d y:%d w:%d h:%d",
         win->popup->x, win->popup->y, win->popup->w, win->popup->h);
   else
     DBG("after_position_calc : x:%d y:%d w:%d h:%d", win->x, win->y, win->w, win->h);
   */


   if (win->fake_iwin)
     {
	win->fake_iwin->x = win->x;
	win->fake_iwin->y = win->y;
	win->fake_iwin->w = win->w;
	win->fake_iwin->h = win->h;
     }
 
   if(win->popup)
     {
        evas_object_move(ng->clip, 0, win->popup->h - ng->opt.edge_offset);
        evas_object_resize(ng->clip, win->popup->w, ng->opt.edge_offset - ng->opt.reflection_offset);
        
        evas_object_move(ng->bg_clip, 0, 0);
        evas_object_resize(ng->bg_clip, win->popup->w, win->popup->h);
     }
   
   if(win->fake_iwin)
     {
        evas_object_move(ng->clip, 0, win->h - ng->opt.edge_offset);
        evas_object_resize(ng->clip, win->w, ng->opt.edge_offset - ng->opt.reflection_offset);
        
        evas_object_move(ng->bg_clip, 0, 0);
        evas_object_resize(ng->bg_clip, win->w, win->h);
     }
}

static Eina_Bool
_ngi_mouse_in_timer(void *data)
{
   Ng *ng = data;

   ng->mouse_in_timer = NULL;
   ng->mouse_in = 1;

   ngi_input_extents_calc(ng);

   ngi_animate(ng);

   if(ng->cfg->show_label)
     {
        if (ng->item_active && ng->item_active->label && ng->item_active->label[0])
          {
             evas_object_show(ng->o_label);
             edje_object_signal_emit(ng->o_label, "e,state,label,show", "e");
             edje_object_part_text_set(ng->o_label, "e.text.label", ng->item_active->label);
          }
     }

   return EINA_FALSE;
}

void
ngi_mouse_in(Ng *ng)
{
   if (ng->mouse_in_timer)
     ecore_timer_del(ng->mouse_in_timer);

   if (ng->hide_state == hidden)
     ng->mouse_in_timer = ecore_timer_add(0.15, _ngi_mouse_in_timer, ng);
   else
     _ngi_mouse_in_timer(ng);
}

static Eina_Bool
_ngi_win_cb_mouse_in(void *data, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Mouse_In *ev = event;
   Ng *ng = data;

   if (ev->win != ng->win->input)
      return EINA_TRUE;

   if (!ecore_x_screen_is_composited(e_manager_current_get()->num) &&
       _ngi_win_border_intersects(ng))
     {
        ng->cfg->shaped_set = EINA_TRUE;
        ecore_event_add(ENGAGE_EVENT_WIN_CHANGE, NULL, NULL, NULL);
     }

   //INF("Ecore_X_Event_Mouse_In");
   ng->pos = ng->horizontal ?
      (ev->root.x - ng->zone->x) :
      (ev->root.y - ng->zone->y);

   ngi_mouse_in(ng);


   if (!ngi_config->use_composite)
     evas_event_feed_mouse_in(ng->evas, ev->time, NULL);

   return EINA_TRUE;
}

void
ngi_mouse_out(Ng *ng)
{
   ng->mouse_in = 0;

   if (ng->mouse_in_timer)
     ecore_timer_del(ng->mouse_in_timer);
   ng->mouse_in_timer = NULL;

   ngi_item_mouse_out(ng->item_active);
   edje_object_signal_emit(ng->o_label, "e,state,label,hide", "e");

   ngi_animate(ng);
}

static Eina_Bool
_ngi_win_cb_mouse_out(void *data, int type EINA_UNUSED, void *event)
{
   Ecore_X_Event_Mouse_Out *ev = event;
   Ng *ng = data;

   if (ev->win != ng->win->input)
      return EINA_TRUE;

   if ((((mouse_win_x >= 1) && (mouse_win_x <= (int)ng->win->rect.width)) ||
       ((mouse_win_y >= 1) && (mouse_win_y <= (int)ng->win->rect.height))) &&
       (ng->cfg->shaped_set))
     return EINA_TRUE;

   ngi_mouse_out(ng);

   if (ng->cfg->shaped_set)
     ng->hide = EINA_TRUE;

   if (!ngi_config->use_composite)
     evas_event_feed_mouse_out(ng->evas, ev->time, NULL);

   return EINA_TRUE;
}

static Eina_Bool
_ngi_menu_wait_after_timer_cb(void *data)
{
   Ng *ng = data;

   ng->menu_wait_timer = NULL;
   ngi_animate(ng);

   return EINA_FALSE;
}

static Eina_Bool
_ngi_menu_wait_timer_cb(void *data)
{
   Ng *ng = data;

   if (e_menu_grab_window_get())
     {
        edje_object_signal_emit(ng->o_label, "e,state,label,hide", "e");
	return EINA_TRUE;
     }

   ng->menu_wait_timer =
      ecore_timer_add(1.0, _ngi_menu_wait_after_timer_cb, ng);

   return EINA_FALSE;
}

static Eina_Bool
_ngi_win_cb_mouse_down(void *data, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Mouse_Button *ev = event;
   Ng *ng = data;

   if (ev->event_window != ng->win->input)
      return EINA_TRUE;

   Ngi_Item *it = ng->item_active;

   if (ev->buttons == 2)
     {
        ngi_configure_module(ng->cfg);
        return EINA_TRUE;
     }
   else if (ng->item_active && ev->buttons == 1)
     {
        it->drag.x = ev->root.x;
        it->drag.y = ev->root.y;
        it->drag.start = 1;
        it->drag.dnd = 0;

	if (it->cb_drag_start)
	  ng->item_drag = it;
     }

   ngi_item_mouse_down(it, ev);

   if (!ngi_config->use_composite)
     {
	Evas_Button_Flags flags = EVAS_BUTTON_NONE;
	if (ev->double_click) flags |= EVAS_BUTTON_DOUBLE_CLICK;
	if (ev->triple_click) flags |= EVAS_BUTTON_TRIPLE_CLICK;
	evas_event_feed_mouse_down(ng->evas, ev->buttons, flags, ev->timestamp, NULL);
     }

   /* if a grab window appears shortly after clicking the
      bar it probably is a menu that belongs to a item */
   if (ng->menu_wait_timer)
      ecore_timer_del(ng->menu_wait_timer);

   ng->menu_wait_timer =
      ecore_timer_add(0.1, _ngi_menu_wait_timer_cb, ng);

   return EINA_TRUE;
}

static Eina_Bool
_ngi_win_cb_mouse_up(void *data, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Mouse_Button *ev = event;
   Ng *ng = data;

   if (ev->event_window != ng->win->input)
      return EINA_TRUE;

   if (ng->item_active)
     {
        ngi_item_mouse_up(ng->item_active, ev);
	evas_event_feed_mouse_up(ng->evas, ev->buttons, EVAS_BUTTON_NONE, ev->timestamp, NULL);

        if (ng->item_drag)
          {
             ng->item_drag->drag.start = 0;
             ng->item_drag = NULL;
          }
     }

   return EINA_TRUE;
}

static Eina_Bool
_ngi_win_cb_mouse_wheel(void *data, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Mouse_Wheel *ev = event;
   Ng *ng = data;

   if (ev->event_window != ng->win->input)
      return EINA_TRUE;

   if (!ngi_config->use_composite)
     evas_event_feed_mouse_wheel(ng->evas, ev->direction, ev->z, ev->timestamp, NULL);

   return EINA_TRUE;
}

static Eina_Bool
_ngi_win_cb_mouse_move(void *data, int type EINA_UNUSED, void *event)
{
   Ecore_Event_Mouse_Move *ev = event;
   Ng *ng = data;

   if (ev->event_window != ng->win->input)
      return EINA_TRUE;

   ng->pos = ng->horizontal ? ev->root.x : ev->root.y;

   ng->pos -= ng->horizontal ? ng->zone->x : ng->zone->y;

   //WRN("X:%d :: Y:%d RECT: %dx%d", ev->x, ev->y, ng->win->rect.width, ng->win->rect.height);
   if (!ng->mouse_in)
     return EINA_TRUE;

   ngi_item_activate(ng);

   if (!ngi_config->use_composite)
     evas_event_feed_mouse_move(ng->evas,
				ev->x + ng->win->rect.x,
				ev->y + ng->win->rect.y,
				ev->timestamp, NULL);

   if (ng->item_drag)
     {
        int dx = ev->root.x - ng->item_drag->drag.x;
        int dy = ev->root.y - ng->item_drag->drag.y;

        if (((dx * dx) + (dy * dy)) > (e_config->drag_resist * e_config->drag_resist))
          {
             ng->item_drag->drag.dnd = 1;
             ng->item_drag->drag.start = 0;

	     ng->item_drag->cb_drag_start(ng->item_drag);
             ngi_item_mouse_out(ng->item_active);

             ng->item_active = NULL;
             ng->item_drag = NULL;
          }
     }

   ngi_animate(ng);

   return EINA_TRUE;
}

/**************************************************************************/


Ngi_Item *
ngi_item_at_position_get(Ng *ng)
{
   Eina_List *l, *ll;
   Ngi_Item *it;
   Ngi_Box *box;
   int pos = ng->pos;

   EINA_LIST_FOREACH (ng->boxes, ll, box)
     {
	if (pos < box->pos)
	  continue;

	EINA_LIST_FOREACH (box->items, l, it)
	  {
	     if (it->delete_me)
	       continue;

	     if (it->scale == 0.0)
	       continue;

	     if (pos <= it->pos + ng->size + ng->opt.item_spacing)
	       {
		  if (pos + ng->opt.item_spacing < it->pos - ng->size)
		    return NULL;

		  return it;
	       }
	  }
     }
   return NULL;
}

void
ngi_item_activate(Ng *ng)
{
   Ngi_Item *it;

   if (!ng->mouse_in)
     {
	ng->item_active = NULL;
	evas_object_hide(ng->o_label);
	return;
     }

   if ((it = ngi_item_at_position_get(ng)))
     {
	if (it != ng->item_active)
	  {
	     ngi_item_mouse_out(ng->item_active);
	     ngi_item_mouse_in(it);
	     ng->item_active = it;
	     _ngi_label_pos_set(ng);

             if(ng->cfg->show_label)
               {
                  if (it->label)
                    {                  
                       evas_object_show(ng->o_label);
                       edje_object_signal_emit(ng->o_label, "e,state,label,show", "e");
                       edje_object_part_text_set(ng->o_label, "e.text.label", it->label);
                    }
                  else
                    {
                       evas_object_hide(ng->o_label);
                    }
               }
	  }
     }
   else
     {
        ngi_item_mouse_out(ng->item_active);
        ng->item_active = NULL;
	evas_object_hide(ng->o_label);
     }
}

/**************************************************************************/

static void
_ngi_item_appear(Ng *ng, Ngi_Item *it)
{
   it->scale = (ecore_time_get() - it->start_time) / ng->opt.fade_duration;

   if (it->scale < 1.0)
     {
        ngi_reposition(ng);
	return;
     }

   it->scale = 1.0;
   ng->items_show = eina_list_remove(ng->items_show, it);

   ngi_reposition(ng);
   ngi_input_extents_calc(ng);
   /* ngi_item_activate(ng); */
}

static void
_ngi_item_disappear(Ng *ng, Ngi_Item *it)
{
   it->scale = 1.0 - (ecore_time_get() - it->start_time) / ng->opt.fade_duration;

   if (it->scale > 0.0)
     {
        ngi_reposition(ng);
	return;
     }

   ng->items_remove = eina_list_remove(ng->items_remove, it);

   ngi_item_free(it);

   ng->size = ng->cfg->size;
   ngi_reposition(ng);
   ngi_input_extents_calc(ng);
   ngi_animate(ng);
   /* ngi_item_activate(ng); */
}

static double
_ngi_anim_advance_in(double start, double duration)
{
   double z = (ecore_time_get() - start) / duration;

   if (z > 1.0)
     return 1.0;

   if (z < 0.0)
     return 0.0;

   z = log(10) * z;

   return 1.0 - (1.0 / exp(z*z));
}

static double
_ngi_anim_advance_out(double start, double duration)
{
   double z = (ecore_time_get() - start) / duration;

   if (z > 1.0)
     return 0.0;

   if (z < 0.0)
     return 1.0;

   z = 1.0 - z;
   z = log(10) * z;

   return 1.0 - (1.0 / exp(z*z));
}

static void
_ngi_zoom_in(Ng *ng)
{
   double z;

   if (ng->state != zooming)
     {
	double now = ecore_time_get();

	if (ng->state == unzooming)
	  ng->start_zoom = now - (ng->cfg->zoom_duration - (now - ng->start_zoom));
	else
	  ng->start_zoom = now;

	ng->state = zooming;
     }

   if (!ng->cfg->shaped_set)
     z = _ngi_anim_advance_in(ng->start_zoom, ng->cfg->zoom_duration);
   else
     z = _ngi_anim_advance_in(ng->start_zoom, 0.60);

   ng->zoom = 1.0 + (ng->cfg->zoomfactor - 1.0) * z;

   if (z == 1.0)
     {
        ng->zoom = ng->cfg->zoomfactor;
        ng->state = zoomed;
     }
}

static void
_ngi_zoom_out(Ng *ng)
{
   double z;

   if (ng->state != unzooming)
     {
	double now = ecore_time_get();

	if ((ng->state == zooming) && !ng->cfg->shaped_set)
	  ng->start_zoom = now - (ng->cfg->zoom_duration - (now - ng->start_zoom));
        else if ((ng->state == zooming) && ng->cfg->shaped_set)
	  ng->start_zoom = now - (0.60 - (now - ng->start_zoom));
	else
	  ng->start_zoom = now;

	ng->state = unzooming;
     }

   if (!ng->cfg->shaped_set)
     z = _ngi_anim_advance_out(ng->start_zoom, ng->cfg->zoom_duration);
   else
     z = _ngi_anim_advance_out(ng->start_zoom, 0.60);

   ng->zoom = 1.0 + (ng->cfg->zoomfactor - 1.0) * z;

   if (z == 0.0)
     {
        ng->zoom = 1.0;
        ng->state = unzoomed;

	if (!ng->hide)
	  ngi_input_extents_calc(ng);

        if (ng->item_active)
          {
             ngi_item_mouse_out(ng->item_active);
             ng->item_active = NULL;
          }
     }
}

static int
_ngi_autohide(Ng *ng, int hide)
{
   double duration = ng->cfg->zoom_duration;
   double step = ng->hide_step;
   double hide_max = ng->size + ng->opt.edge_offset + ng->opt.bg_offset;

   if (ng->cfg->shaped_set) duration = 0.60;

   if (hide)
     {
	if (ng->hide_state != hiding)
	  {
	     double now = ecore_time_get();

	     if (ng->hide_state == showing)
	       ng->start_hide = now - (step / hide_max) * duration;
	     else
	       ng->start_hide = now;

	     if (ng->start_hide < now - duration)
	       ng->start_hide = now - duration;

	     ng->hide_state = hiding;
	  }

	step = hide_max * (1.0 - _ngi_anim_advance_out(ng->start_hide, duration));

	if (step >= hide_max)
	  {
	     ng->hide_state = hidden;
	     ng->hide_step = hide_max;
	     ngi_input_extents_calc(ng);

             if (ng->shaped_active && ng->cfg->shaped_set)
               {
                  ng->cfg->shaped_set = EINA_FALSE;
                  ecore_event_add(ENGAGE_EVENT_WIN_CHANGE, NULL, NULL, NULL);
               }

	     return 0;
	  }
     }
   else
     {
	if (ng->hide_state != showing)
	  {
	     double now = ecore_time_get();

	     if (ng->hide_state == hiding)
	       ng->start_hide = now - (duration - (step / hide_max) * duration);
	     else
	       ng->start_hide = now;

	     ng->hide_state = showing;
	  }

	step = hide_max * (1.0 - _ngi_anim_advance_in(ng->start_hide, duration));

	if (step <= 0)
	  {
	     ng->hide_state = show;
	     ng->hide_step = 0;
	     ngi_input_extents_calc(ng);
	     return 0;
	  }
     }

   if (step > hide_max)
     step = hide_max;
   else if (step < 0)
     step = 0;

   ng->hide_step = step;

   return 1;
}

void
ngi_animate(Ng *ng)
{
   ng->changed = 1;
   if (ng->animator)
      return;

   ng->animator = ecore_animator_add(_ngi_animator, ng);
}

static Eina_Bool
_ngi_animator(void *data)
{
   Ng *ng = data;
   Eina_Bool cont = 0;
   Eina_List *l, *ll;
   Ngi_Item *it;

   if (ng->items_remove || ng->items_show)
     cont = 1;

   EINA_LIST_FOREACH_SAFE(ng->items_show, l, ll, it)
     _ngi_item_appear(ng, it);

   EINA_LIST_FOREACH_SAFE(ng->items_remove, l, ll, it)
     _ngi_item_disappear(ng, it);

   if (ng->cfg->zoomfactor > 1.0)
     {
        if (ng->mouse_in && (ng->state != zoomed))
          {
	     _ngi_zoom_in(ng);
	     cont = 1;
          }
        else if (!ng->mouse_in && (ng->state != unzoomed))
          {
	     _ngi_zoom_out(ng);
	     cont = 1;
          }
     }

   if (ng->cfg->autohide)
     {
        if (ng->mouse_in || ng->show_bar || ng->menu_wait_timer || (!ng->hide))
          {
             if (ng->hide_state != show)
               {
		  _ngi_autohide(ng, 0);
                  cont = 1;
               }
          }
        else
          {
             if ((ng->hide_state != hidden) &&
		 ((ng->zoom - 1.0) <= (ng->cfg->zoomfactor - 1.0) / 2.0))
               {
		  _ngi_autohide(ng, 1);
		  cont = 1;
               }
          }
     }

   if (cont || ng->changed)
     {
	_ngi_redraw(ng);
	return EINA_TRUE;
     }

   ng->animator = NULL;
   return EINA_FALSE;
}

void
ngi_reposition(Ng *ng)
{
   Ngi_Box *box;
   double pos;
   Eina_List *l, *ll;
   Ngi_Item *item;
   int size = ng->size;
   int cnt = 0, end;
   int width = 0;

   if(ng->win->popup)
      width = ng->horizontal ? ng->win->popup->w : ng->win->popup->h;

   if(ng->win->fake_iwin)
      width = ng->horizontal ? ng->win->w : ng->win->h;

   for (;;)
     {
	ng->w = 0;

	EINA_LIST_FOREACH (ng->boxes, l, box)
	  {
	     box->w = 0;

	     EINA_LIST_FOREACH (box->items, ll, item)
	       {
		  if (item->scale == 0.0) continue;
		  box->w += (size * item->scale) + ng->opt.item_spacing;
	       }

	     ng->w += box->w;

	     if (cnt++ > 0) ng->w += ng->opt.separator_width;
	  }

	ng->start = (width - ng->w) / 2;

	/* distance = ng->start - SIDE_OFFSET - width/2; */

	end = _ngi_zoom_function(ng, width/2, ng->start - SIDE_OFFSET);

	if ((end > 0) || (size <= 16))
	  break;

	/* shrink bar when it becomes larger than screen height/width  */
	ng->size = size = size - 1;
     }

   cnt = 0;
   pos = ng->start;

   EINA_LIST_FOREACH (ng->boxes, l, box)
   {
      box->pos = pos;

      if (cnt++ > 0)
         pos += (ng->opt.separator_width);

      EINA_LIST_FOREACH (box->items, ll, item)
      {
         if (item->scale == 0.0)
            continue;

         item->pos = pos;

         pos += item->scale * size + ng->opt.item_spacing;
      }
   }

   if (!ng->cfg->ecomorph_features)
     return;

   if (ng->items_show || ng->items_remove)
      return;

   /* FIXME */
   EINA_LIST_FOREACH (ng->boxes, l, box)
     {
	Ngi_Item_Taskbar *it;

	if (!(box->cfg->type == taskbar))
	  continue;

	switch(ng->cfg->orient)
	  {
	   case E_GADCON_ORIENT_BOTTOM:
	      EINA_LIST_FOREACH(box->items, ll, it)
                {
                   if(ng->win->popup)
                     _ngi_netwm_icon_geometry_set
                        (it->border, it->base.pos, 
                         (ng->win->popup->y + ng->win->popup->h) - size, 
                         size, size);

                   if(ng->win->fake_iwin)
                     _ngi_netwm_icon_geometry_set
                        (it->border, it->base.pos, 
                         (ng->win->y + ng->win->h) - size, 
                         size, size);
                }
	      break;

	   case E_GADCON_ORIENT_TOP:
	      EINA_LIST_FOREACH(box->items, ll, it)
		_ngi_netwm_icon_geometry_set
		(it->border, it->base.pos,
		 ng->hide_step, size, size);
	      break;

	   case E_GADCON_ORIENT_LEFT:
	      EINA_LIST_FOREACH(box->items, ll, it)
		_ngi_netwm_icon_geometry_set
		(it->border, ng->hide_step,
		 it->base.pos, size, size);
	      break;

	   case E_GADCON_ORIENT_RIGHT:
	      EINA_LIST_FOREACH(box->items, ll, it)
                {
                   if(ng->win->popup)
                     _ngi_netwm_icon_geometry_set
                        (it->border, 
                         (ng->win->popup->x + ng->win->popup->w) - size, 
                         it->base.pos, size, size);

                   if(ng->win->fake_iwin)
                     _ngi_netwm_icon_geometry_set
                        (it->border, 
                         (ng->win->x + ng->win->w) - size, 
                         it->base.pos, size, size);
                }
	      break;
	  }
     }
}


static double
_ngi_zoom_function(Ng *ng, double to, double pos)
{
   double range = ng->cfg->zoom_range * ng->size;
   double d = pos - to;

   if (ng->zoom <= 1.0)
     {
	return pos;
     }

   if (d >= range)
     {
	return pos + range * (ng->zoom - 1.0);
     }
   else if (d <= -range)
     {
	return pos + -range * (ng->zoom - 1.0);
     }
   else
     {
	/* erf(distance / range) * range * (ng->cfg->zoomfactor - 1.0); */

	return pos + sin(M_PI/2.0 * d/range) * range * (ng->zoom - 1.0);
     }

   return pos;
}

static void
_ngi_label_pos_set(Ng *ng)
{
   int off;

   if (!ng->item_active)
     return;

   off = (ng->size * ng->zoom) + ng->opt.edge_offset + TEXT_DIST;

   switch (ng->cfg->orient)
     {
      case E_GADCON_ORIENT_BOTTOM:
         if(ng->win->popup)
           evas_object_move(ng->o_label, ng->item_active->pos + ng->size/2, (ng->win->popup->h + ng->hide_step) - off);

         if(ng->win->fake_iwin)
           evas_object_move(ng->o_label, ng->item_active->pos + ng->size/2, (ng->win->h + ng->hide_step) - off);
	 break;

      case E_GADCON_ORIENT_TOP:
	 evas_object_move(ng->o_label, ng->item_active->pos + ng->size/2,
			  (off - ng->hide_step));
	 break;
      case E_GADCON_ORIENT_RIGHT:

         if(ng->win->popup)
           evas_object_move(ng->o_label, (ng->win->popup->w + ng->hide_step) - off, ng->item_active->pos + ng->size/2);

         if(ng->win->fake_iwin)
           evas_object_move(ng->o_label, (ng->win->w + ng->hide_step) - off, ng->item_active->pos + ng->size/2);
	 break;

      case E_GADCON_ORIENT_LEFT:
	 evas_object_move(ng->o_label, (off - ng->hide_step),
			  ng->item_active->pos + ng->size/2);
	 break;
     }
}

static void
_ngi_redraw(Ng *ng)
{
   Ngi_Item *it;
   Ngi_Box *box;
   Eina_List *l, *ll;

   double pos, pos2;
   int end1, end2, size_spacing, hide_step;
   int bw = 0, bh = 0, bx = 0, by = 0;
   int w = 0, h = 0;

   Config_Item *cfg = ng->cfg;

   int edge_offset = ng->opt.edge_offset;
   int separator_width = ng->opt.separator_width;

   int cnt = 0;
   /* double disp = 0.0; */

   if(ng->win->popup)
     {
        w = ng->win->popup->w;
        h = ng->win->popup->h;
     }

   if(ng->win->fake_iwin)
     {
        w = ng->win->w;
        h = ng->win->h;
     }

   if (cfg->autohide)
     hide_step = ng->hide_step;
   else
     hide_step = ng->hide_step = 0;

   size_spacing = ng->size + edge_offset;

   _ngi_label_pos_set(ng);

   if (cfg->show_background)
     {
        end1 = _ngi_zoom_function(ng, ng->pos, ng->start);
        end2 = _ngi_zoom_function(ng, ng->pos, ng->start + ng->w);

        switch (cfg->orient)
          {
           case E_GADCON_ORIENT_BOTTOM:
	      bx = end1;
	      by = h + hide_step - size_spacing;
	      bw = end2 - end1;
	      bh = size_spacing;
              break;

           case E_GADCON_ORIENT_TOP:
	      bx = end1;
	      by = - hide_step;
	      bw = end2 - end1;
	      bh = size_spacing;
              break;

           case  E_GADCON_ORIENT_LEFT:
	      bx = - hide_step;
	      by = end1;
	      bw = size_spacing;
	      bh = end2 - end1;
              break;

           case E_GADCON_ORIENT_RIGHT:
	      bx = w + hide_step - size_spacing;
	      by = end1;
	      bw = size_spacing;
	      bh = end2 - end1;
          }

	evas_object_move(ng->o_bg, bx, by);
	evas_object_resize(ng->o_bg, bw, bh);
	evas_object_move(ng->o_frame, bx, by);
	evas_object_resize(ng->o_frame, bw, bh);
     }

   EINA_LIST_FOREACH (ng->boxes, ll, box)
   {
      if (cnt++ > 0)
        {
           pos = _ngi_zoom_function(ng, ng->pos, box->pos);

           switch (cfg->orient)
             {
              case E_GADCON_ORIENT_BOTTOM:
                 evas_object_move(box->separator, pos, h + hide_step - size_spacing);
                 evas_object_resize(box->separator, separator_width, size_spacing);
                 break;

              case E_GADCON_ORIENT_TOP:
                 evas_object_move(box->separator, pos, -hide_step);
                 evas_object_resize(box->separator, separator_width, size_spacing);
                 break;

              case E_GADCON_ORIENT_LEFT:
                 evas_object_move(box->separator, -hide_step, pos);
                 evas_object_resize(box->separator, size_spacing, separator_width);
                 break;

              case E_GADCON_ORIENT_RIGHT:
                 evas_object_move(box->separator, w + hide_step - size_spacing, pos);
                 evas_object_resize(box->separator, size_spacing, separator_width);
             }
           evas_object_show(box->separator);
        }
      else
         evas_object_hide(box->separator);

      pos2 = 0;

      EINA_LIST_FOREACH (box->items, l, it)
	{
	   double size;
	   int off;

	   if (it->scale == 0.0)
	     continue;

	   if (pos2 == 0)
	     pos = _ngi_zoom_function(ng, ng->pos, it->pos);
	   else
	     pos = pos2 + (double)ng->opt.item_spacing;

	   pos2 = _ngi_zoom_function(ng, ng->pos, it->pos + (ng->size * it->scale));

	   size = (int)pos2 - (int)pos;

	   off = edge_offset + (1.0 - it->scale) * (ng->size)/2.0;

	   switch (cfg->orient)
	     {
	      case E_GADCON_ORIENT_BOTTOM:
		 bx = pos;
		 by = h + hide_step - (off + size);
		 break;

	      case E_GADCON_ORIENT_TOP:
		 bx = pos;
		 by = off - hide_step;
		 break;

	      case E_GADCON_ORIENT_LEFT:
		 bx = off - hide_step;
		 by = pos;
		 break;

	      case E_GADCON_ORIENT_RIGHT:
		 bx = w + hide_step - (off + size);
		 by = pos;
		 break;
	     }

	   evas_object_move(it->obj,  bx, by);
	   evas_object_resize(it->obj, size, size);

	   evas_object_move(it->over, bx, by);
	   evas_object_resize(it->over, size, size);
	}
   }

   ng->changed = 0;
}

void
ngi_bar_lock(Ng *ng, int display)
{
   if (!ng) return;

   if (display)
     {
	ng->show_bar++;
     }
   else if (ng->show_bar > 0)
     {
	ng->show_bar--;
     }
}

void
ngi_freeze(Ng *ng)
{
   if (ng->animator)
     {
        ecore_animator_del(ng->animator);
        ng->animator = NULL;
     }
}

void
ngi_thaw(Ng *ng)
{
   ngi_reposition(ng);
   ngi_input_extents_calc(ng);
   ngi_animate(ng);
}

static Eina_Bool
_ngi_win_border_intersects(Ng *ng)
{
   Eina_List *l;
   E_Border *bd;
   E_Desk *desk;
   int x = 0, y = 0, w = 0, h = 0, size;

   desk = e_desk_current_get(ng->zone);
   size = ng->size + ng->opt.bg_offset + ng->opt.edge_offset;

   switch (ng->cfg->orient)
     {
      case E_GADCON_ORIENT_BOTTOM:
	 x = ng->start;
         if(ng->win->popup)
           y = ng->win->popup->h - size;

         if(ng->win->fake_iwin)
           y = ng->win->h - size;

	 w = ng->w;
	 h = size;
	 break;

      case E_GADCON_ORIENT_TOP:
	 x = ng->start;
	 y = 0;
	 w = ng->w;
	 h = size;
	 break;

      case E_GADCON_ORIENT_LEFT:
	 x = 0;
	 y = ng->start;
	 w = size;
	 h = ng->w;
	 break;

      case E_GADCON_ORIENT_RIGHT:
         if(ng->win->popup)
           x = ng->win->popup->w - size;

         if(ng->win->fake_iwin)
           x = ng->win->w - size;

	 y = ng->start;
	 w = size;
	 h = ng->w;
	 break;
     }

   if(ng->win->popup)
     {
        x += ng->zone->x + ng->win->popup->x;
        y += ng->zone->y + ng->win->popup->y;
     }
   else if(ng->win->fake_iwin)
     {
        x += ng->zone->x + ng->win->x;
        y += ng->zone->y + ng->win->y;
     }

   EINA_LIST_FOREACH(e_border_client_list(), l, bd)
     {
	if (bd->delete_requested)
	  continue;

	if (bd->zone != ng->zone)
	  continue;

	if ((bd->desk != desk) && (!bd->sticky))
	  continue;

	if (bd->iconic)
	  continue;

	if (!bd->visible)
	  continue;

	if (E_INTERSECTS(x, y, w, h, bd->x, bd->y, bd->w, bd->h))
	  return EINA_TRUE;
     }

   return EINA_FALSE;
}

static void
_ngi_win_autohide_check(Ng *ng, E_Desk *desk)
{
   int hide;

   if (desk->zone != ng->zone)
     return;

   if (ng->cfg->stacking == ENGAGE_BELOW_FULLSCREEN)
     {
	hide = desk->fullscreen_borders;

	if(ng->win->popup)
          {
             if (hide)
               e_popup_hide(ng->win->popup);
             else
               e_popup_show(ng->win->popup);
          }
     }

   if (ng->cfg->autohide == AUTOHIDE_FULLSCREEN)
     {
	hide = desk->fullscreen_borders;

	if (ng->hide != hide)
	  ngi_animate(ng);

	ng->hide = hide;
     }
   else if (ng->cfg->autohide == AUTOHIDE_OVERLAP)
     {
	hide = _ngi_win_border_intersects(ng);

	if (ng->hide != hide)
	  ngi_animate(ng);

	ng->hide = hide;
     }
}

static Eina_Bool
_ngi_win_cb_desk_show(void *data, int type EINA_UNUSED, void *event)
{
   E_Event_Desk_Show *ev = event;
   Ng *ng = data;

   _ngi_win_autohide_check(ng, ev->desk);

   return EINA_TRUE;
}

static Eina_Bool
_ngi_win_cb_border_event(void *data, int type EINA_UNUSED, void *event)
{
   E_Event_Border_Property *ev = event;
   Ng *ng = data;
   E_Desk *desk;

   desk = e_desk_current_get(ng->zone);

   if ((ev->border->desk == desk) || (ev->border->sticky))
     _ngi_win_autohide_check(ng, desk);

   return EINA_TRUE;
}

/***************************************************************************/

EAPI E_Module_Api e_modapi = { E_MODULE_API_VERSION, "engage" };

static Eina_Bool
_ngi_init_timer_cb(void *data EINA_UNUSED)
{
   Eina_List *l;
   Config_Item *ci;

   if (ecore_x_screen_is_composited(e_manager_current_get()->num))
     ngi_config->use_composite = EINA_TRUE;

   EINA_LIST_FOREACH (ngi_config->items, l, ci)
     ngi_new(ci);

   initialized = 1;

   return ECORE_CALLBACK_CANCEL;
}

/* menu item callback(s) */
/*
static void
_e_mod_run_cb(void *data EINA_UNUSED, E_Menu *m EINA_UNUSED, E_Menu_Item *mi EINA_UNUSED)
{
   ngi_instances_config(e_util_zone_current_get(e_manager_current_get())->container, NULL);
}
*/
/* menu item add hook */
/*
static void
_e_mod_menu_add(void *data EINA_UNUSED, E_Menu *m)
{
   E_Menu_Item *mi;

   mi = e_menu_item_new(m);
   e_menu_item_label_set(mi, D_("Engage"));
   e_util_menu_item_theme_icon_set(mi, "preferences-desktop-shelf");
   e_menu_item_callback_set(mi, _e_mod_run_cb, NULL);
}
*/

static void
_ngi_config_free()
{
   Config_Item *ci;
   Config_Box *cfg_box;
   Config_Gadcon *cg;

   EINA_LIST_FREE(ngi_config->items, ci)
     {
        EINA_LIST_FREE(ci->boxes, cfg_box)
	  {
	     if (cfg_box->type == launcher && cfg_box->launcher_app_dir)
	       eina_stringshare_del(cfg_box->launcher_app_dir);

	     if (cfg_box->type == gadcon)
	       {
		  EINA_LIST_FREE(cfg_box->gadcon_items, cg)
		    eina_stringshare_del(cg->name);
	       }
	  }
        free(ci);
     }

   if (ngi_config->theme_path) free(ngi_config->theme_path);
   free(ngi_config);
   ngi_config = NULL;
}

Config_Item *
ngi_bar_config_new(int container_num, int zone_num)
{
   Config_Item *cfg;
   Config_Box *cfg_box;
   char buf[4096];
   char tmp[4096];
   FILE *f;
   char *app_dir = "default";

   cfg = E_NEW(Config_Item, 1);
   cfg->show_label = 1;
   cfg->show_background = 0;
   cfg->container = container_num;
   cfg->zone = zone_num;
   cfg->orient = E_GADCON_ORIENT_BOTTOM;
   cfg->size = 32;
   cfg->autohide = AUTOHIDE_OVERLAP;
   cfg->autohide_show_urgent = 0;
   cfg->hide_below_windows = 1;
   cfg->zoom_duration = 0.4;
   cfg->zoom_range = 2.1;
   cfg->hide_timeout = 0.1;
   cfg->zoomfactor = 2.5;
   cfg->alpha = 255;
   cfg->stacking = ENGAGE_BELOW_FULLSCREEN;
   cfg->old_stacking = cfg->stacking;
   cfg->mouse_over_anim = 1;
   cfg->lock_deskswitch = 1;
   cfg->ecomorph_features = 1;
   cfg->boxes = NULL;

   cfg_box = E_NEW(Config_Box, 1);
   cfg_box->type = launcher;
   cfg_box->launcher_app_dir = eina_stringshare_add(app_dir);
   cfg_box->launcher_lock_dnd = 1;
   cfg->boxes = eina_list_append(cfg->boxes, cfg_box);

   cfg_box = E_NEW(Config_Box, 1);
   cfg_box->type = taskbar;
   cfg_box->taskbar_adv_bordermenu = 1;
   cfg_box->taskbar_skip_dialogs = 1;
   cfg_box->taskbar_show_iconified = 1;
   cfg_box->taskbar_show_desktop = 0;
   cfg_box->taskbar_append_right = 1;
   cfg_box->taskbar_group_apps = 1;
   cfg->boxes = eina_list_append(cfg->boxes, cfg_box);

   snprintf(buf, sizeof(buf), "%s/applications/bar/%s",
	    e_user_dir_get(), app_dir);

   if (!ecore_file_exists(buf))
     {
	ecore_file_mkdir(buf);
	snprintf(buf, sizeof(buf), "%s/applications/bar/%s/.order",
		 e_user_dir_get(), app_dir);
	f = fopen(buf, "w");
	if (f)
	  {
	     snprintf(tmp, sizeof(tmp),
		      "xterm.desktop\n"
		      "firefox.desktop\n"
		      "gimp.desktop\n");
	     fwrite(tmp, sizeof(char), strlen(tmp), f);
	     fclose(f);
	  }
     }

   ngi_config->items = eina_list_append(ngi_config->items, cfg);

   return cfg;
}

EAPI void *
e_modapi_init(E_Module *m)
{
   char buf[PATH_MAX];
   ngi_config = NULL;

   engage_log = eina_log_domain_register("MOD:ENGAGE", EINA_COLOR_LIGHTBLUE);
   eina_log_domain_level_set("MOD:ENGAGE", EINA_LOG_LEVEL_DBG);
   INF("Initialized Engage Module");

   /* Location of message catalogs for localization */
   snprintf(buf, sizeof(buf), "%s/locale", e_module_dir_get(m));
   bindtextdomain(PACKAGE, buf);
   bind_textdomain_codeset(PACKAGE, "UTF-8");

   ngi_conf_gadcon_edd = E_CONFIG_DD_NEW("Ngi_Config_Gadcon", Config_Gadcon);
   E_CONFIG_VAL(ngi_conf_gadcon_edd, Config_Gadcon, name, STR);

   ngi_conf_box_edd = E_CONFIG_DD_NEW("Ngi_Config_Item_Box", Config_Box);
   E_CONFIG_VAL(ngi_conf_box_edd, Config_Box, type, INT);
   E_CONFIG_VAL(ngi_conf_box_edd, Config_Box, launcher_app_dir, STR);
   E_CONFIG_VAL(ngi_conf_box_edd, Config_Box, launcher_lock_dnd, INT);
   E_CONFIG_VAL(ngi_conf_box_edd, Config_Box, taskbar_skip_dialogs, INT);
   E_CONFIG_VAL(ngi_conf_box_edd, Config_Box, taskbar_adv_bordermenu, INT);
   E_CONFIG_VAL(ngi_conf_box_edd, Config_Box, taskbar_show_iconified, INT);
   E_CONFIG_VAL(ngi_conf_box_edd, Config_Box, taskbar_show_desktop, INT);
   E_CONFIG_VAL(ngi_conf_box_edd, Config_Box, taskbar_append_right, INT);
   E_CONFIG_VAL(ngi_conf_box_edd, Config_Box, taskbar_group_apps, INT);
   E_CONFIG_LIST(ngi_conf_box_edd, Config_Box, gadcon_items, ngi_conf_gadcon_edd);

   ngi_conf_item_edd = E_CONFIG_DD_NEW("Ngi_Config_Item", Config_Item);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, show_label, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, show_background, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, container, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, zone, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, orient, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, size, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, autohide, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, autohide_show_urgent, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, hide_timeout, FLOAT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, zoom_duration, FLOAT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, zoomfactor, FLOAT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, zoom_range, FLOAT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, hide_below_windows, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, alpha, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, stacking, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, old_stacking, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, mouse_over_anim, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, lock_deskswitch, INT);
   E_CONFIG_VAL(ngi_conf_item_edd, Config_Item, ecomorph_features, INT);
   E_CONFIG_LIST(ngi_conf_item_edd, Config_Item, boxes, ngi_conf_box_edd);

   ngi_conf_edd = E_CONFIG_DD_NEW("Ngi_Config", Config);
   E_CONFIG_VAL(ngi_conf_edd, Config, version, INT);
   E_CONFIG_LIST(ngi_conf_edd, Config, items, ngi_conf_item_edd);

   ngi_config = (Config *)e_config_domain_load("module.engage", ngi_conf_edd);

   if (ngi_config && !e_util_module_config_check(D_("Engage"),
						 ngi_config->version,
						 MOD_CONFIG_FILE_VERSION))
     _ngi_config_free();

   if (!ngi_config)
     {
	ngi_config = E_NEW(Config, 1);
	ngi_config->version = (MOD_CONFIG_FILE_EPOCH << 16);

	ngi_bar_config_new(0, 0);
     }

   ngi_config->cfd = NULL;
   ngi_config->module = m;

   snprintf(buf, sizeof(buf), "%s/engage.edj", e_module_dir_get(m));
   ngi_config->theme_path = strdup(buf);

   snprintf(buf, sizeof(buf), "%s/applications/bar/", e_user_dir_get());
   if (!ecore_file_mkdir(buf) && !ecore_file_is_dir(buf))
     {
        e_error_message_show("Error creating directory:\n %s\n",buf);
        return m;
     }

   e_configure_registry_item_add("applications/engage", 40,
                                 D_("Engage"), NULL,
                                 "preferences-desktop-shelf",
                                 ngi_configure_one);

   ngi_config->handlers = eina_list_append
     (ngi_config->handlers, ecore_event_handler_add
      (E_EVENT_CONTAINER_RESIZE, _ngi_cb_container_resize, NULL));

   ngi_taskbar_init();
   ngi_gadcon_init();

   e_module_delayed_set(m, 1);

   ecore_idler_add(_ngi_init_timer_cb, NULL);

   /* _ngi_init_timer_cb(NULL); */

   /* maug = e_int_menus_menu_augmentation_add
    *   ("config/1", _e_mod_menu_add, NULL, NULL, NULL); */

   return m;
}

EAPI int
e_modapi_shutdown(E_Module *m EINA_UNUSED)
{
   Ecore_Event_Handler *h;
   Ng *ng;
   Eina_List *l, *ll;

   
   if (maug)
     {
        e_int_menus_menu_augmentation_del("config/1", maug);
        maug = NULL;
     }

   ngi_gadcon_shutdown();

   EINA_LIST_FOREACH_SAFE(ngi_config->instances, l, ll, ng)
     {
	if (ng->cfg->config_dialog)
	  e_object_del(E_OBJECT(ng->cfg->config_dialog));

        if (ng->composite_timer)
          ecore_timer_del(ng->composite_timer);
        
        if (ng->force_timer)
          ecore_timer_del(ng->force_timer); 

	ngi_free(ng);
     }

   EINA_LIST_FREE(ngi_config->handlers, h)
     ecore_event_handler_del(h);

   _ngi_config_free();

   E_CONFIG_DD_FREE(ngi_conf_gadcon_edd);
   E_CONFIG_DD_FREE(ngi_conf_box_edd);
   E_CONFIG_DD_FREE(ngi_conf_item_edd);
   E_CONFIG_DD_FREE(ngi_conf_edd);

   e_configure_registry_item_del("applications/engage");

   return 1;
}

EAPI int
e_modapi_save(E_Module *m EINA_UNUSED)
{
   e_config_domain_save("module.engage", ngi_conf_edd, ngi_config);

   return 1;
}
