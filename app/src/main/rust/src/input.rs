use {
    glam::Mat4,
    openxr::{self as xr, Space },
};

pub struct Actions {
    action_set: xr::ActionSet,
    right_pos: xr::Action<xr::Posef>,
    left_pos: xr::Action<xr::Posef>,
    right_click: xr::Action<f32>,
    left_click: xr::Action<f32>,

    left_thumbstick: xr::Action<xr::Vector2f>,
    right_thumbstick: xr::Action<xr::Vector2f>,
}

pub struct Spaces {
    right_space: Space,
    left_space: Space,
}

pub struct InputState {
    actions: Actions,
    spaces: Spaces,
}

/// (note) if you need any new inputs, put them here and extract them in InputState::extract
pub struct ExtractedInputs {
    pub movement: [f32; 3], // world-space (i.e., needs to be transformed into camera-relative before it's really useful)
    pub left_click_state: bool,
    pub right_click_state: bool,

    pub left_hand_matrix: Option<Mat4>,
    pub right_hand_matrix: Option<Mat4>,
}

impl InputState {
    pub fn new(instance: &xr::Instance, session: &xr::Session<xr::Vulkan>) -> Self {
        let action_set = instance
            .create_action_set("input", "Input Pose Information", 0)
            .unwrap();
        let right_pos = action_set
            .create_action::<xr::Posef>("right_hand", "Right Hand Controller", &[])
            .unwrap();
        let left_pos = action_set
            .create_action::<xr::Posef>("left_hand", "Left Hand Controller", &[])
            .unwrap();
        let right_click = action_set
            .create_action::<f32>("right_click", "Right Hand Click", &[])
            .unwrap();
        let left_click = action_set
            .create_action::<f32>("left_click", "Left Hand Click", &[])
            .unwrap();
        let right_thumbstick = action_set
            .create_action::<xr::Vector2f>("right_thumbstick", "Right Thumbstick", &[])
            .unwrap();
        let left_thumbstick = action_set
            .create_action::<xr::Vector2f>("left_thumbstick", "Left Thumbstick", &[])
            .unwrap();
        instance.suggest_interaction_profile_bindings(
            instance.string_to_path("/interaction_profiles/oculus/touch_controller")
                .unwrap(),
            &[
                xr::Binding::new(&right_pos, instance.string_to_path("/user/hand/right/input/aim/pose").unwrap()),
                xr::Binding::new(&left_pos, instance.string_to_path("/user/hand/left/input/aim/pose").unwrap()),
                xr::Binding::new(&right_thumbstick, instance.string_to_path("/user/hand/right/input/thumbstick").unwrap()),
                xr::Binding::new(&left_thumbstick, instance.string_to_path("/user/hand/left/input/thumbstick").unwrap()),
                xr::Binding::new(&right_click, instance.string_to_path("/user/hand/right/input/trigger/value").unwrap()),
                xr::Binding::new(&left_click, instance.string_to_path("/user/hand/left/input/trigger/value").unwrap()),
            ]
        ).unwrap();
        session.attach_action_sets(&[&action_set]).unwrap();

        let right_space = right_pos
            .create_space(&session, xr::Path::NULL, xr::Posef::IDENTITY)
            .unwrap();
        let left_space = left_pos
            .create_space(&session, xr::Path::NULL, xr::Posef::IDENTITY)
            .unwrap();

        InputState {
            actions: Actions {
                action_set,
                left_pos,
                right_pos,
                left_click,
                right_click,
                left_thumbstick,
                right_thumbstick
            },
            spaces: Spaces {
                right_space,
                left_space,
            },
        }
    }

    pub fn extract(&self, session: &xr::Session<xr::Vulkan>, stage: &Space, predicted_display_time: xr::Time) -> ExtractedInputs {
        let active_action_sets = [xr::ActiveActionSet::new(&self.actions.action_set)];
        if let Err(e) = session.sync_actions(&active_action_sets) {
            log::error!("Failed to sync actions: {:?}", e);
        }

        let mut movement = [0.0f32; 3];
        if let Ok(state) = self.actions.right_thumbstick.state(session, xr::Path::NULL) {
            movement[0] = state.current_state.x;
            movement[2] = -state.current_state.y;
        }

        if let Ok(state) = self.actions.left_thumbstick.state(session, xr::Path::NULL) {
            movement[1] = -state.current_state.y; // for some reason down on the thumbstick makes y positive? idk
        }

        let mut right_click_state = false;
        if let Ok(state) = self.actions.right_click.state(session, xr::Path::NULL) {
            right_click_state = state.current_state > 0.5;
        }
        let mut left_click_state = false;
        if let Ok(state) = self.actions.left_click.state(session, xr::Path::NULL) {
            left_click_state = state.current_state > 0.5;
        }

        let left_hand_matrix = self.actions.left_pos
            .is_active(session, xr::Path::NULL)
            .ok()
            .and_then(|active| active.then(|| self.spaces.left_space.locate(stage, predicted_display_time).ok()))
            .flatten()
            .filter(|location| {
                location.location_flags.contains(
                    xr::SpaceLocationFlags::POSITION_VALID | xr::SpaceLocationFlags::ORIENTATION_VALID,
                )
            })
            .map(|location| crate::xr_util::pose_to_matrix(&location.pose));
        let right_hand_matrix = self.actions.right_pos
            .is_active(session, xr::Path::NULL)
            .ok()
            .and_then(|active| active.then(|| self.spaces.right_space.locate(stage, predicted_display_time).ok()))
            .flatten()
            .filter(|location| {
                location.location_flags.contains(
                    xr::SpaceLocationFlags::POSITION_VALID | xr::SpaceLocationFlags::ORIENTATION_VALID,
                )
            })
            .map(|location| crate::xr_util::pose_to_matrix(&location.pose));

        ExtractedInputs {
            movement,
            left_click_state,
            right_click_state,
            left_hand_matrix,
            right_hand_matrix,
        }
    }
}