import torch
import torch.nn as nn

class LSTMAnomalyPredictor(nn.Module):
    def __init__(self, input_size=4, hidden_size=64, num_layers=2, output_size=1):
        """
        LSTM for predicting structural integrity based on 50 frames of historical telemetry.
        Inputs: [Altitude, Velocity, Dynamic Pressure, Vibration/G-Force]
        Output: Predicted Structural Integrity Percentage [0.0, 1.0]
        """
        super(LSTMAnomalyPredictor, self).__init__()
        self.hidden_size = hidden_size
        self.num_layers = num_layers
        
        # LSTM layer taking the time-series sequence
        self.lstm = nn.LSTM(input_size, hidden_size, num_layers, batch_first=True)
        
        # Fully connected layers to decode LSTM state to structural integrity %
        self.fc = nn.Sequential(
            nn.Linear(hidden_size, 32),
            nn.ReLU(),
            nn.Linear(32, output_size),
            nn.Sigmoid() # Bound output between 0 and 1
        )
        
    def forward(self, x):
        """
        x shape: (batch_size, sequence_length=50, input_size=4)
        Returns: (batch_size, 1) predicted integrity at t+5s
        """
        # Initialize hidden state and cell state
        h0 = torch.zeros(self.num_layers, x.size(0), self.hidden_size).to(x.device)
        c0 = torch.zeros(self.num_layers, x.size(0), self.hidden_size).to(x.device)
        
        # Forward propagate LSTM
        out, _ = self.lstm(x, (h0, c0))
        
        # Decode the hidden state of the last time step
        out = self.fc(out[:, -1, :])
        return out
