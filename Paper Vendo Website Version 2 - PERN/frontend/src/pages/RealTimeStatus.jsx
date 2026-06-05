import React, { useState, useEffect } from 'react';
import axios from 'axios';
import { 
  Tv, 
  Coins, 
  Cpu, 
  RotateCcw, 
  Activity, 
  Radio, 
  Info, 
  AlertCircle,
  HelpCircle,
  Play
} from 'lucide-react';

export default function RealTimeStatus() {
  const [status, setStatus] = useState({
    coins_inserted: 0.00,
    credits_remaining: 0.00,
    selected_type: 'None',
    selected_brand: 'None',
    selected_size: 'None',
    oled_display_text: 'Smart Vendo V3\nInsert Coin',
    scale_weight_grams: 0.00,
    ir_sensor_blocked: false,
    stepper_position_steps: 0,
    servo_angle_change: 0,
    updated_at: new Date().toISOString()
  });

  const [loading, setLoading] = useState(true);
  const [updating, setUpdating] = useState(false);
  const [logs, setLogs] = useState([
    { time: new Date().toLocaleTimeString(), type: 'system', message: 'Admin cloud monitor panel linked.' }
  ]);

  const addLog = (type, message) => {
    setLogs(prev => [
      { time: new Date().toLocaleTimeString(), type, message },
      ...prev.slice(0, 49) // Keep last 50 logs
    ]);
  };

  const fetchRealTimeStatus = async () => {
    try {
      const res = await axios.get('/api/machine/realtime');
      setStatus(res.data);
    } catch (err) {
      console.error('Error fetching realtime status:', err);
    } finally {
      setLoading(false);
    }
  };

  // Poll status every 3 seconds for live-updating dashboard feel
  useEffect(() => {
    fetchRealTimeStatus();
    const interval = setInterval(fetchRealTimeStatus, 3000);
    return () => clearInterval(interval);
  }, []);

  const updateStatusInDb = async (updatedFields) => {
    setUpdating(true);
    try {
      const payload = { ...status, ...updatedFields };
      const res = await axios.put('/api/machine/realtime', payload);
      setStatus(res.data.data);
    } catch (err) {
      console.error('Failed to update status in database:', err);
      addLog('error', 'Cloud sync failed. Check connection.');
    } finally {
      setUpdating(false);
    }
  };

  // --- MOCK SIMULATOR ACTIONS ---
  
  // 1. Insert Coin
  const simulateInsertCoin = async (amount) => {
    const newCoins = parseFloat(status.coins_inserted) + amount;
    const newCredits = parseFloat(status.credits_remaining) + amount;
    
    addLog('sensor', `Coin slot pulse: +₱${amount}.00 coin accepted.`);
    
    let oledText = `CREDITS: ₱${newCredits.toFixed(2)}\nInsert Coin / A/B`;
    if (status.selected_type !== 'None') {
      oledText = `CREDITS: ₱${newCredits.toFixed(2)}\nSelected: ${status.selected_type.toUpperCase()}`;
    }

    await updateStatusInDb({
      coins_inserted: newCoins,
      credits_remaining: newCredits,
      oled_display_text: oledText
    });
  };

  // 2. Clear Credits / Reset Coins
  const resetCoinsSimulator = async () => {
    addLog('system', 'Reset simulator coins and credit totals.');
    await updateStatusInDb({
      coins_inserted: 0.00,
      credits_remaining: 0.00,
      selected_type: 'None',
      selected_brand: 'None',
      selected_size: 'None',
      oled_display_text: 'Smart Vendo V3\nInsert Coin',
      ir_sensor_blocked: false,
      stepper_position_steps: 0,
      servo_angle_change: 0
    });
  };

  // 3. Keypad Requests
  const simulateKeypress = async (key) => {
    addLog('keypad', `Keypad Key pressed: [${key}]`);

    // Verify credits
    if (status.credits_remaining < 1.00) {
      addLog('warning', `Request rejected: credits too low (₱${status.credits_remaining})`);
      await updateStatusInDb({
        oled_display_text: 'ERR: LOW CREDIT\nInsert Coin first'
      });
      setTimeout(() => {
        updateStatusInDb({
          oled_display_text: `CREDITS: ₱${status.credits_remaining.toFixed(2)}\nInsert Coin / Key`
        });
      }, 2000);
      return;
    }

    // Process Keypad Selection
    if (key >= '1' && key <= '8') {
      // Paper Specs
      const paperSizeNames = {
        '1': '1/4 (Budget White)',
        '2': 'Crosswise (Budget White)',
        '3': 'Lengthwise (Budget White)',
        '4': '1-Whole (Budget White)',
        '5': '1/4 (Standard Yellow)',
        '6': 'Crosswise (Standard Yellow)',
        '7': 'Lengthwise (Standard Yellow)',
        '8': '1-Whole (Standard Yellow)'
      };
      
      const sizeSelected = paperSizeNames[key];
      addLog('api', `Querying database for paper specs ID: ${key}`);
      
      await updateStatusInDb({
        selected_type: 'paper',
        selected_brand: key <= '4' ? 'Budget Brand (White)' : 'Standard Brand (Yellow)',
        selected_size: key === '1' || key === '5' ? '1/4' : key === '2' || key === '6' ? 'crosswise' : key === '3' || key === '7' ? 'lengthwise' : '1_whole',
        oled_display_text: `Checking Cloud...\nPaper layout: ${key}`
      });

      // Simulate API verification
      setTimeout(async () => {
        const cost = 1.00; // Default budget sheet cost
        const sheetsAlloc = 4;
        
        addLog('actuator', `DB validated. Dispensing ${sheetsAlloc} sheets of ${sizeSelected}`);
        await updateStatusInDb({
          oled_display_text: `Dispensing...\nQty: ${sheetsAlloc} sheets`,
          scale_weight_grams: Math.max(status.scale_weight_grams - 8.5, 0) // reduce weight of paper stack
        });

        // Complete dispense
        setTimeout(async () => {
          const change = status.credits_remaining - cost;
          addLog('actuator', `Dispense complete. Change return: ₱${change.toFixed(2)}`);
          
          await updateStatusInDb({
            credits_remaining: 0, // Dispensed all credits
            selected_type: 'None',
            selected_brand: 'None',
            selected_size: 'None',
            servo_angle_change: change > 0 ? 90 : 0, // Activate change dispenser servo
            oled_display_text: change > 0 ? `Returning change\n₱${change.toFixed(2)}` : 'Thank you!\nSmart Vendo V3'
          });

          // Reset change return servo
          if (change > 0) {
            setTimeout(() => {
              updateStatusInDb({
                servo_angle_change: 0,
                oled_display_text: 'Smart Vendo V3\nInsert Coin'
              });
            }, 3000);
          } else {
            setTimeout(() => {
              updateStatusInDb({
                oled_display_text: 'Smart Vendo V3\nInsert Coin'
              });
            }, 3000);
          }
        }, 3000);

      }, 1500);

    } else if (key === 'A' || key === 'B') {
      // Pen specs
      const penName = key === 'A' ? 'Budget Ballpen' : 'Standard Ballpen';
      const cost = key === 'A' ? 5.00 : 10.00;

      addLog('api', `Querying database for ballpen specs ID: ${key === 'A' ? 1 : 2}`);

      if (status.credits_remaining < cost) {
        addLog('warning', `Insufficient credits: ${penName} requires ₱${cost.toFixed(2)}`);
        await updateStatusInDb({
          oled_display_text: `ERR: NEED ₱${cost.toFixed(2)}\nCredits: ₱${status.credits_remaining.toFixed(2)}`
        });
        setTimeout(() => {
          updateStatusInDb({
            oled_display_text: `CREDITS: ₱${status.credits_remaining.toFixed(2)}\nInsert Coin / Key`
          });
        }, 2000);
        return;
      }

      await updateStatusInDb({
        selected_type: 'pen',
        selected_brand: penName,
        selected_size: 'Standard',
        oled_display_text: `Checking Cloud...\nPen item: ${key}`
      });

      // Simulate pen drop
      setTimeout(async () => {
        addLog('actuator', `DB validated. Triggering Stepper to drop position (180 deg)`);
        await updateStatusInDb({
          oled_display_text: 'Dispensing...\nWaiting for drop',
          stepper_position_steps: 1024
        });

        // Trigger IR sensor block
        setTimeout(async () => {
          addLog('sensor', 'IR Sensor status: OBSTRUCTED (Pen drop detected)');
          await updateStatusInDb({
            ir_sensor_blocked: true,
            oled_display_text: 'Pen Dispensed!\nThank you'
          });

          // Finish dispense, return change
          setTimeout(async () => {
            const change = status.credits_remaining - cost;
            addLog('actuator', `Dispense complete. Return Stepper to zero. Change return: ₱${change.toFixed(2)}`);
            
            await updateStatusInDb({
              credits_remaining: 0,
              selected_type: 'None',
              selected_brand: 'None',
              selected_size: 'None',
              ir_sensor_blocked: false,
              stepper_position_steps: 0,
              servo_angle_change: change > 0 ? 90 : 0,
              oled_display_text: change > 0 ? `Returning change\n₱${change.toFixed(2)}` : 'Thank you!\nSmart Vendo V3'
            });

            if (change > 0) {
              setTimeout(() => {
                updateStatusInDb({
                  servo_angle_change: 0,
                  oled_display_text: 'Smart Vendo V3\nInsert Coin'
                });
              }, 3000);
            } else {
              setTimeout(() => {
                updateStatusInDb({
                  oled_display_text: 'Smart Vendo V3\nInsert Coin'
                });
              }, 3000);
            }
          }, 2000);

        }, 1500);

      }, 1500);

    } else if (key === '0') {
      // Coin Return
      if (status.credits_remaining <= 0) {
        addLog('warning', 'Cancel request rejected: No credits to return.');
        return;
      }
      
      const coinsToReturn = status.credits_remaining;
      addLog('actuator', `Coin return request. Releasing ₱${coinsToReturn.toFixed(2)} via Servo.`);
      
      await updateStatusInDb({
        oled_display_text: `Returning change\n₱${coinsToReturn.toFixed(2)}`,
        servo_angle_change: 90
      });

      setTimeout(async () => {
        await updateStatusInDb({
          coins_inserted: 0,
          credits_remaining: 0,
          servo_angle_change: 0,
          oled_display_text: 'Smart Vendo V3\nInsert Coin'
        });
        addLog('system', 'Credits returned. Hardware idle.');
      }, 3000);

    } else if (key === '*' || key === '#') {
      // Stepper Nudge
      const dir = key === '*' ? 'FORWARD' : 'BACKWARD';
      const steps = key === '*' ? 20 : -20;
      
      addLog('actuator', `Stepper manual nudge command: ${dir} (${steps} steps)`);
      await updateStatusInDb({
        stepper_position_steps: status.stepper_position_steps + steps,
        oled_display_text: `Stepper Nudge:\n${dir} (${steps} steps)`
      });

      setTimeout(() => {
        updateStatusInDb({
          oled_display_text: `CREDITS: ₱${status.credits_remaining.toFixed(2)}\nInsert Coin / Key`
        });
      }, 1500);
    }
  };

  // Helper to simulate manual weight adjustments
  const adjustLoadCell = async (direction) => {
    let delta = direction === 'up' ? 25 : -25;
    const newWeight = Math.max(parseFloat(status.scale_weight_grams) + delta, 0);
    addLog('sensor', `HX711 Load Cell calibration weight adjusted to: ${newWeight}g`);
    await updateStatusInDb({ scale_weight_grams: newWeight });
  };

  if (loading) {
    return (
      <div className="flex h-[70vh] items-center justify-center">
        <div className="h-10 w-10 animate-spin rounded-full border-4 border-primary-200 border-t-primary-500"></div>
      </div>
    );
  }

  return (
    <div className="space-y-8 max-w-7xl mx-auto font-sans">
      
      {/* Top Header */}
      <div className="flex flex-col md:flex-row md:items-center justify-between gap-4">
        <div>
          <h1 className="font-display font-extrabold text-3xl md:text-4xl text-slate-800 dark:text-white leading-tight">
            Real-Time Monitor
          </h1>
          <p className="text-slate-500 dark:text-slate-400 text-sm mt-1">
            Display local machine status, track hardware sensor readings, and simulate OLED output.
          </p>
        </div>
        <div className="flex items-center gap-2 text-xs bg-primary-500/10 text-primary-500 px-3.5 py-2 rounded-xl border border-primary-500/20 font-semibold">
          <span className="relative flex h-2 w-2">
            <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-primary-450 opacity-75"></span>
            <span className="relative inline-flex rounded-full h-2 w-2 bg-primary-500"></span>
          </span>
          <span>Live Cloud Sync Active</span>
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-12 gap-8">
        
        {/* LEFT COLUMN: Hardware Screen & Sensors (7 columns) */}
        <div className="lg:col-span-7 space-y-6">
          
          {/* OLED DISPLAY SIMULATOR */}
          <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm flex flex-col">
            <div className="flex items-center gap-2 mb-4">
              <Tv className="w-5 h-5 text-primary-500" />
              <h3 className="font-display font-bold text-base text-slate-800 dark:text-white">OLED Screen Simulator</h3>
              <span className="ml-auto text-[10px] uppercase font-bold tracking-wider px-2 py-0.5 rounded bg-slate-105 text-slate-400 dark:bg-white/[0.04]">
                Student View Screen
              </span>
            </div>

            {/* Simulated 128x64 pixel layout screen */}
            <div className="w-full aspect-[21/9] bg-slate-900 border-4 border-slate-700 dark:border-slate-850 rounded-xl flex items-center justify-center p-6 relative overflow-hidden shadow-inner select-none">
              {/* Screen Glass Glare */}
              <div className="absolute top-0 right-0 w-1/2 h-full bg-gradient-to-tr from-transparent to-white/[0.02] transform skew-x-12 pointer-events-none"></div>
              
              {/* Scanlines Effect */}
              <div className="absolute inset-0 bg-scanlines pointer-events-none opacity-20"></div>

              {/* Pixel Font Output */}
              <div className="text-center font-mono text-cyan-400 text-base md:text-xl lg:text-2xl leading-relaxed tracking-wider drop-shadow-[0_0_8px_rgba(34,211,238,0.5)] whitespace-pre-line animate-flicker uppercase">
                {status.oled_display_text}
              </div>

              {/* Status Bar Indicators */}
              <div className="absolute bottom-2 left-4 right-4 flex justify-between text-[9px] font-mono text-cyan-500/60 uppercase">
                <span>Credits: ₱{parseFloat(status.credits_remaining).toFixed(2)}</span>
                <span>Smart Vendo OS</span>
              </div>
            </div>
            <div className="mt-3 text-xs text-slate-400 dark:text-slate-500 flex items-start gap-1.5 p-2 bg-slate-50 dark:bg-white/[0.01] rounded-xl border border-slate-100 dark:border-white/[0.03]">
              <Info className="w-4 h-4 text-primary-500 shrink-0 mt-0.5" />
              <p>This panel simulates the 0.96-inch OLED screen placed on the front of the physical machine. It shows inserted credits, user options, or dispensing details.</p>
            </div>
          </div>

          {/* SENSOR INDICATORS PANEL */}
          <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
            <h3 className="font-display font-bold text-base text-slate-800 dark:text-white mb-6 flex items-center gap-2">
              <Cpu className="w-5 h-5 text-primary-500" />
              <span>Machine Sensor Diagnostics</span>
            </h3>

            <div className="grid grid-cols-1 sm:grid-cols-2 gap-6">
              
              {/* HX711 Weight Scale */}
              <div className="p-4 rounded-xl border border-slate-100 dark:border-white/[0.03] bg-slate-50 dark:bg-white/[0.01]">
                <div className="flex justify-between items-center mb-2">
                  <span className="text-xs font-bold text-slate-400 uppercase tracking-wider">HX711 Load Cell</span>
                  <span className="text-xs font-semibold text-primary-500">Weight Stack</span>
                </div>
                <div className="text-2xl font-extrabold text-slate-800 dark:text-white font-display">
                  {parseFloat(status.scale_weight_grams).toFixed(1)}g
                </div>
                <p className="text-[10px] text-slate-400 dark:text-slate-500 mt-1">
                  Checks stack availability. Approx. ~110 sheets remaining.
                </p>
                
                {/* Manual Calibration buttons in diagnostics */}
                <div className="flex gap-2 mt-3 pt-3 border-t border-slate-200/50 dark:border-white/[0.02]">
                  <button 
                    onClick={() => adjustLoadCell('down')}
                    className="flex-1 text-[10px] font-bold py-1 px-2 rounded-lg bg-slate-200/50 hover:bg-slate-200 dark:bg-white/[0.03] dark:hover:bg-white/[0.06] transition-colors"
                  >
                    -25g (Remove Paper)
                  </button>
                  <button 
                    onClick={() => adjustLoadCell('up')}
                    className="flex-1 text-[10px] font-bold py-1 px-2 rounded-lg bg-slate-200/50 hover:bg-slate-200 dark:bg-white/[0.03] dark:hover:bg-white/[0.06] transition-colors"
                  >
                    +25g (Refill Paper)
                  </button>
                </div>
              </div>

              {/* IR Drop Sensor */}
              <div className="p-4 rounded-xl border border-slate-100 dark:border-white/[0.03] bg-slate-50 dark:bg-white/[0.01] flex flex-col justify-between">
                <div>
                  <div className="flex justify-between items-center mb-2">
                    <span className="text-xs font-bold text-slate-400 uppercase tracking-wider">IR Beam Sensor</span>
                    <span className={`h-2 w-2 rounded-full ${status.ir_sensor_blocked ? 'bg-amber-500 animate-pulse' : 'bg-emerald-500'}`}></span>
                  </div>
                  <div className="flex items-center gap-2 text-2xl font-extrabold text-slate-800 dark:text-white font-display">
                    <span className={status.ir_sensor_blocked ? 'text-amber-500' : 'text-emerald-500'}>
                      {status.ir_sensor_blocked ? 'OBSTRUCTED' : 'CLEAR'}
                    </span>
                  </div>
                  <p className="text-[10px] text-slate-400 dark:text-slate-500 mt-1">
                    Detects falling ballpen dispenser drops to log checkout logs.
                  </p>
                </div>
              </div>

              {/* Stepper Motor */}
              <div className="p-4 rounded-xl border border-slate-100 dark:border-white/[0.03] bg-slate-50 dark:bg-white/[0.01]">
                <div className="flex justify-between items-center mb-2">
                  <span className="text-xs font-bold text-slate-400 uppercase tracking-wider">Stepper Motor (Pen)</span>
                  <span className="text-xs font-semibold text-primary-500">28BYJ-48 Stepper</span>
                </div>
                <div className="text-2xl font-extrabold text-slate-800 dark:text-white font-display">
                  {status.stepper_position_steps} steps
                </div>
                <div className="flex items-center gap-2 mt-1">
                  <div className="flex-1 h-1.5 rounded-full bg-slate-100 dark:bg-white/10 overflow-hidden">
                    <div 
                      className="h-full bg-primary-500 transition-all duration-300"
                      style={{ width: `${Math.min(Math.abs(status.stepper_position_steps / 10.24), 100)}%` }}
                    />
                  </div>
                  <span className="text-[10px] font-bold text-slate-400">
                    {Math.round(status.stepper_position_steps / 5.68)}°
                  </span>
                </div>
              </div>

              {/* Change Returning Servo */}
              <div className="p-4 rounded-xl border border-slate-100 dark:border-white/[0.03] bg-slate-50 dark:bg-white/[0.01]">
                <div className="flex justify-between items-center mb-2">
                  <span className="text-xs font-bold text-slate-400 uppercase tracking-wider">Change Release Servo</span>
                  <span className="text-xs font-semibold text-primary-500">SG90 Servo</span>
                </div>
                <div className="text-2xl font-extrabold text-slate-800 dark:text-white font-display">
                  {status.servo_angle_change}°
                </div>
                <div className="flex items-center gap-2 mt-1">
                  <div className="flex-1 h-1.5 rounded-full bg-slate-100 dark:bg-white/10 overflow-hidden">
                    <div 
                      className="h-full bg-emerald-500 transition-all duration-300"
                      style={{ width: `${(status.servo_angle_change / 180) * 100}%` }}
                    />
                  </div>
                  <span className="text-[10px] font-bold text-slate-400">
                    {status.servo_angle_change === 90 ? 'RELEASEING' : 'HOLDING'}
                  </span>
                </div>
              </div>

            </div>
          </div>

        </div>

        {/* RIGHT COLUMN: Simulation Controls & Event Logger (5 columns) */}
        <div className="lg:col-span-5 space-y-6">
          
          {/* SIMULATION CONTROL DECK */}
          <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm">
            <div className="flex items-center justify-between mb-4">
              <h3 className="font-display font-bold text-base text-slate-800 dark:text-white flex items-center gap-2">
                <Activity className="w-5 h-5 text-primary-500" />
                <span>Simulation Panel</span>
              </h3>
              <button 
                onClick={resetCoinsSimulator}
                disabled={updating}
                className="flex items-center gap-1 text-[11px] font-bold text-red-500 hover:bg-red-50 dark:hover:bg-red-550/10 px-2 py-1 rounded-lg transition-colors border border-red-500/10"
              >
                <RotateCcw className="w-3 h-3" />
                Reset Machine
              </button>
            </div>

            {/* Coins slot mock triggers */}
            <div className="space-y-4">
              <div className="space-y-1.5">
                <span className="text-xs font-bold text-slate-400 uppercase tracking-wider block">1. Coin Acceptor Slot</span>
                <div className="grid grid-cols-3 gap-2">
                  <button 
                    disabled={updating}
                    onClick={() => simulateInsertCoin(1.00)}
                    className="h-10 text-xs font-bold rounded-xl bg-slate-50 dark:bg-white/[0.02] border border-slate-205 dark:border-white/[0.06] hover:bg-primary-500/10 hover:border-primary-500/30 text-slate-700 dark:text-slate-350 hover:text-primary-500 transition-all flex items-center justify-center gap-1 shadow-sm active:scale-[0.97]"
                  >
                    <Coins className="w-3.5 h-3.5" />
                    +₱1 Coin
                  </button>
                  <button 
                    disabled={updating}
                    onClick={() => simulateInsertCoin(5.00)}
                    className="h-10 text-xs font-bold rounded-xl bg-slate-50 dark:bg-white/[0.02] border border-slate-205 dark:border-white/[0.06] hover:bg-primary-500/10 hover:border-primary-500/30 text-slate-700 dark:text-slate-350 hover:text-primary-500 transition-all flex items-center justify-center gap-1 shadow-sm active:scale-[0.97]"
                  >
                    <Coins className="w-3.5 h-3.5" />
                    +₱5 Coin
                  </button>
                  <button 
                    disabled={updating}
                    onClick={() => simulateInsertCoin(10.00)}
                    className="h-10 text-xs font-bold rounded-xl bg-slate-50 dark:bg-white/[0.02] border border-slate-205 dark:border-white/[0.06] hover:bg-primary-500/10 hover:border-primary-500/30 text-slate-700 dark:text-slate-350 hover:text-primary-500 transition-all flex items-center justify-center gap-1 shadow-sm active:scale-[0.97]"
                  >
                    <Coins className="w-3.5 h-3.5" />
                    +₱10 Coin
                  </button>
                </div>
              </div>

              {/* Physical Keypad Press Simulators */}
              <div className="space-y-1.5 pt-2 border-t border-slate-200/50 dark:border-white/[0.02]">
                <span className="text-xs font-bold text-slate-400 uppercase tracking-wider block">2. 4x4 Membrane Keypad</span>
                
                <div className="grid grid-cols-4 gap-2 max-w-[280px] mx-auto p-3 bg-slate-100 dark:bg-slate-900/50 rounded-2xl border border-slate-200/60 dark:border-white/[0.04] shadow-inner">
                  {/* Row 1 */}
                  {['1', '2', '3', 'A'].map(k => (
                    <button 
                      key={k}
                      disabled={updating}
                      onClick={() => simulateKeypress(k)}
                      className={`h-11 rounded-xl text-sm font-black flex items-center justify-center shadow-sm active:scale-[0.93] transition-all ${
                        k === 'A' 
                          ? 'bg-primary-500 text-white hover:bg-primary-600' 
                          : 'bg-white dark:bg-[#1e293b] border border-slate-200 dark:border-white/[0.04] hover:bg-slate-50 dark:hover:bg-white/[0.03] text-slate-700 dark:text-slate-300'
                      }`}
                    >
                      {k}
                    </button>
                  ))}
                  
                  {/* Row 2 */}
                  {['4', '5', '6', 'B'].map(k => (
                    <button 
                      key={k}
                      disabled={updating}
                      onClick={() => simulateKeypress(k)}
                      className={`h-11 rounded-xl text-sm font-black flex items-center justify-center shadow-sm active:scale-[0.93] transition-all ${
                        k === 'B' 
                          ? 'bg-emerald-500 text-white hover:bg-emerald-600' 
                          : 'bg-white dark:bg-[#1e293b] border border-slate-200 dark:border-white/[0.04] hover:bg-slate-50 dark:hover:bg-white/[0.03] text-slate-700 dark:text-slate-300'
                      }`}
                    >
                      {k}
                    </button>
                  ))}

                  {/* Row 3 */}
                  {['7', '8', '9', 'C'].map(k => (
                    <button 
                      key={k}
                      disabled={updating}
                      onClick={() => simulateKeypress(k)}
                      className="h-11 rounded-xl text-sm font-black flex items-center justify-center bg-white dark:bg-[#1e293b] border border-slate-200 dark:border-white/[0.04] hover:bg-slate-50 dark:hover:bg-white/[0.03] text-slate-700 dark:text-slate-300 shadow-sm active:scale-[0.93] transition-all"
                    >
                      {k}
                    </button>
                  ))}

                  {/* Row 4 */}
                  {['*', '0', '#', 'D'].map(k => (
                    <button 
                      key={k}
                      disabled={updating}
                      onClick={() => simulateKeypress(k)}
                      className={`h-11 rounded-xl text-sm font-black flex items-center justify-center shadow-sm active:scale-[0.93] transition-all ${
                        k === '0' 
                          ? 'bg-amber-500 text-white hover:bg-amber-600' 
                          : k === '*' || k === '#'
                            ? 'bg-slate-200 dark:bg-white/10 hover:bg-slate-300 dark:hover:bg-white/20 text-slate-700 dark:text-slate-300'
                            : 'bg-white dark:bg-[#1e293b] border border-slate-200 dark:border-white/[0.04] hover:bg-slate-50 dark:hover:bg-white/[0.03] text-slate-700 dark:text-slate-300'
                      }`}
                    >
                      {k}
                    </button>
                  ))}
                </div>

                <div className="p-2 border border-slate-100 dark:border-white/[0.03] rounded-xl bg-slate-50 dark:bg-white/[0.01] mt-3">
                  <div className="grid grid-cols-2 gap-x-2 gap-y-1 text-[10px] text-slate-400 dark:text-slate-500 leading-normal font-mono">
                    <div>• 1 - 4: Budget 1/4 - Whole</div>
                    <div>• 5 - 8: Standard 1/4 - Whole</div>
                    <div>• A: Budget Pen (₱5)</div>
                    <div>• B: Standard Pen (₱10)</div>
                    <div>• 0: Return inserted coins</div>
                    <div>• *, #: Nudge Stepper +/-</div>
                  </div>
                </div>
              </div>

            </div>
          </div>

          {/* SIMULATION DIAGNOSTIC LOGGER */}
          <div className="p-6 rounded-2xl bg-white border border-slate-200 dark:bg-[#161F30] dark:border-white/[0.06] shadow-sm flex flex-col h-[280px]">
            <h3 className="font-display font-bold text-base text-slate-800 dark:text-white mb-4 flex items-center gap-2 shrink-0">
              <Radio className="w-5 h-5 text-primary-500" />
              <span>Event Logger Console</span>
            </h3>

            {/* Terminal console printout */}
            <div className="flex-1 bg-slate-900/90 rounded-xl p-4 border border-slate-950 font-mono text-[10.5px] overflow-y-auto space-y-2.5 shadow-inner">
              {logs.map((log, index) => {
                let colorClass = 'text-cyan-400';
                if (log.type === 'sensor') colorClass = 'text-amber-400';
                if (log.type === 'keypad') colorClass = 'text-emerald-400';
                if (log.type === 'api') colorClass = 'text-purple-400';
                if (log.type === 'actuator') colorClass = 'text-pink-400';
                if (log.type === 'warning') colorClass = 'text-rose-400';
                if (log.type === 'error') colorClass = 'text-red-500 font-bold';

                return (
                  <div key={index} className="flex items-start gap-1.5 leading-relaxed">
                    <span className="text-slate-500 shrink-0 select-none">[{log.time}]</span>
                    <span className={`${colorClass} shrink-0 select-none font-bold uppercase`}>{log.type}:</span>
                    <span className="text-slate-300 break-all">{log.message}</span>
                  </div>
                );
              })}
            </div>
          </div>

        </div>

      </div>
    </div>
  );
}
