from pydantic_settings import BaseSettings

class Settings(BaseSettings):
    TICK_RATE_HZ: int = 60
    CORS_ORIGINS: list[str] = ["*"]
    
    class Config:
        env_file = ".env"

settings = Settings()
