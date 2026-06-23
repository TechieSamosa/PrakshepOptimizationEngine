import os
from stable_baselines3 import PPO
from stable_baselines3.common.env_util import make_vec_env
from stable_baselines3.common.callbacks import CheckpointCallback
from ml_environment.rocket_env import PrakshepEnv

def train_ppo():
    # Define environment
    env_id = lambda: PrakshepEnv(rocket_type=0, payload_mass=1750.0)
    
    # Vectorized environment for parallel training
    vec_env = make_vec_env(env_id, n_envs=4)
    
    # Setup Checkpoint Callback
    checkpoint_callback = CheckpointCallback(save_freq=10000, save_path='./logs/ppo_prakshep/', name_prefix='ppo_model')
    
    # Setup PPO Model
    model = PPO(
        "MlpPolicy", 
        vec_env, 
        verbose=1,
        learning_rate=3e-4,
        n_steps=2048,
        batch_size=64,
        n_epochs=10,
        gamma=0.99,
        gae_lambda=0.95,
        clip_range=0.2,
        ent_coef=0.01, # Encourage exploration
        tensorboard_log="./logs/ppo_tensorboard/"
    )
    
    print("Starting PPO Training...")
    # Train Agent
    model.learn(total_timesteps=1_000_000, callback=checkpoint_callback)
    
    # Save Final Model
    model.save("ppo_prakshep_final")
    print("Training complete and model saved.")

if __name__ == '__main__':
    train_ppo()
