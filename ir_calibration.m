function ir_calibration(R)
% IR_CALIBRATION  Monte Carlo calibration of the V1 and V2 sensing/stop logic against bench trials.
%   V1 = spin_detect v1.1        (count-difference alignment, 100 ms windows)
%   V2 = spin_detect_clock v0.5  (centroid PID, 10 ms slices, dead band, hard limit)
%   Run:  ir_calibration        (400 runs per version)   or   ir_calibration(100)
%. Outputs: console tables + fig_sim_vs_measured_matlab.png/.pdf
%
%   MODEL ASSUMPTIONS (unmeasured):
%     cosine beam +/-45 deg; Poisson counts, 72 per 100 ms on-axis (bench log 72/71/71);
%     motor omega = (PWM-60)*1.29 deg/s with a 50 ms first-order lag; sensors 120 deg apart;
%     alignment error = |beacon bearing - laser axis| at rest (laser axis = centre receiver).
if nargin < 1, R = 400; end
if exist('OCTAVE_VERSION','builtin'), rand('twister',5); else, rng(5); end

P.SL = 0.010;  P.tau = 0.05;  P.peak = 72;  P.bg = 0.02;
P.K = 5;                                            % detection threshold (counts / 100 ms)
% ---- V1 constants (spin_detect.cpp) ----
P.spinPWM = 130; P.trackPWM = 110; P.margin = 8; P.clearWin = 2;
P.timeout = 4.0; P.lostWin = 5; P.revPause1 = 0.100;
% ---- V2 constants (spin_detect_clock.cpp) ----
P.minPWM = 100; P.maxPWM = 140; P.Kp = 140; P.Ki = 40; P.Kd = 6; P.Imax = 1;
P.trackThr = 3; P.hyst = 0.10; P.deadA = 0.15; P.deadB = 0.60; P.relax = 2.4;
P.signMin = 0.05; P.stopSlices = 3; P.force = 3.0; P.revPause2 = 0.030;

% ---- measured bench trials (beacon at 1 m, 5 V supply, same start position) ----
M.err  = {[31 35 38], [38 36 24]};     % alignment error [deg]
M.time = {[1.2 1.1 1.4], [0.2 0.4 0.3]}; % detection-to-stop time [s]

T = cell(1,2); E = cell(1,2); F = zeros(1,2); N = zeros(1,2);
for v = 1:2
    t_ = nan(R,1); e_ = nan(R,1); f_ = false(R,1);
    for r = 1:R
        phi = 360*rand;  blocks = true(1,600);        % steady beacon (always on)
        if v == 1, [t_(r), f_(r), e_(r)] = runV1(P, phi, blocks);
        else,      [t_(r), f_(r), e_(r)] = runV2(P, phi, blocks); end
    end
    ok = ~isnan(t_);  T{v} = t_(ok);  E{v} = e_(ok);  F(v) = sum(f_(ok));  N(v) = sum(ok);
end

%% ---- Table 2 (simulation) ----
names = {'V1 spin_detect','V2 spin_detect_clock'};
fprintf('\nTable 2 - simulated results (%d runs per version)\n', R);
fprintf('%-22s %-12s %-14s %-24s %-24s %-8s\n','Version','Stops by test','Ends on limit','Stop time med [p10-p90] s','Alignment med [p10-p90] deg','Mean deg');
for v = 1:2
    fprintf('%-22s %-12d %-14s %-24s %-24s %-8.1f\n', names{v}, N(v)-F(v), sprintf('%d (%.0f%%)',F(v),100*F(v)/N(v)), ...
        sprintf('%.2f [%.2f-%.2f]',pct(T{v},50),pct(T{v},10),pct(T{v},90)), ...
        sprintf('%.1f [%.0f-%.0f]',pct(E{v},50),pct(E{v},10),pct(E{v},90)), mean(E{v}));
end

fprintf('Runs taking longer than 4 s: V1 %.1f%%, V2 %.1f%%  (V1 tail = lost-signal exit back to scanning, then re-detection)\n', ...
    100*mean(T{1}>4), 100*mean(T{2}>4));
fprintf('Runs that stopped immediately at detection (0 s): V1 %.0f%%, V2 %.0f%%\n', 100*mean(T{1}<0.02), 100*mean(T{2}<0.02));

%% ---- Table 3 (measured vs simulation) ----
fprintf('\nTable 3 - measured (n = 3 per version)\n');
for v = 1:2
    fprintf('%s: alignment %.1f +/- %.1f deg | stop time %.2f +/- %.2f s\n', names{v}, mean(M.err{v}), std(M.err{v}), mean(M.time{v}), std(M.time{v}));
end
[tE,dfE,pE] = welch(M.err{1}, M.err{2});   [tT,dfT,pT] = welch(M.time{1}, M.time{2});
fprintf('Alignment change %.1f deg (%.1f%%): Welch t = %.2f, df = %.1f, p = %.2f\n', mean(M.err{2})-mean(M.err{1}), 100*(mean(M.err{2})/mean(M.err{1})-1), tE, dfE, pE);
fprintf('Stop-time change %.2f s (%.1f%%): Welch t = %.2f, df = %.1f, p = %.4f\n\n', mean(M.time{2})-mean(M.time{1}), 100*(mean(M.time{2})/mean(M.time{1})-1), tT, dfT, pT);

%% ---- Figure ----
col = [0.85 0.15 0.15; 0.10 0.35 0.80];
fig = figure('Position',[100 100 880 340],'Color','w');
ax1 = subplot(1,2,1); hold on; grid on; box on;
ax2 = subplot(1,2,2); hold on; grid on; box on;
for v = 1:2
    s = sort(T{v}); plot(ax1, s, (1:numel(s))/numel(s), 'Color', col(v,:), 'LineWidth', 1.5);
    plot(ax1, M.time{v}, (0.04+0.03*(v-1))*ones(1,3), 'o', 'MarkerFaceColor', col(v,:), 'MarkerEdgeColor','k', 'MarkerSize', 6);
    s = sort(E{v}); plot(ax2, s, (1:numel(s))/numel(s), 'Color', col(v,:), 'LineWidth', 1.5);
    plot(ax2, M.err{v}, (0.04+0.03*(v-1))*ones(1,3), 'o', 'MarkerFaceColor', col(v,:), 'MarkerEdgeColor','k', 'MarkerSize', 6);
end
xlabel(ax1,'Time from detection to stop (s)'); ylabel(ax1,'Cumulative fraction of runs'); title(ax1,'(a) Stopping time');
xlabel(ax2,'Laser-axis alignment error at rest (deg)');                                  title(ax2,'(b) Alignment error');
xlim(ax1,[0 5]);   % V1 tail beyond 5 s is not drawn (see console output for its share)
legend(ax1, {'V1 simulated','V1 measured','V2 simulated','V2 measured'}, 'Location','southeast');
print(fig, 'fig_sim_vs_measured_matlab.png', '-dpng', '-r300');
try, print(fig, 'fig_sim_vs_measured_matlab.pdf', '-dpdf'); catch, end
end

%% =============== V1: spin_detect (100 ms windows) ===============
function [tt, forced, err] = runV1(P, phi, blocks)
offs = [0 120 -120];  TURN = [0 1 -1];
th = 0; om = omg(P.spinPWM); t = 0; mdir = 1; pwm = P.spinPWM;
armed = false; clr = 0; turn = false; lost = 0; tFirst = NaN; tStart = 0; forced = false; done = false;
while t < 40 && ~done
    w = [0 0 0];
    for k = 1:10
        [c, th, om, t] = slice(P, phi, blocks, offs, th, om, t, mdir*omg(pwm));
        w = w + c;
    end
    [wb, best] = max(w);  det = wb >= P.K;
    aligned = det && w(1) >= P.K && w(1) + P.margin >= wb;
    if ~turn
        if ~armed
            if det, clr = 0; else, clr = clr + 1; end
            if clr >= P.clearWin, armed = true; end
        elseif det
            if isnan(tFirst), tFirst = t; end
            if aligned
                done = true;
            else
                turn = true; lost = 0; tStart = t;
                [th, om, t, mdir] = setDir(P, phi, blocks, offs, th, om, t, mdir, TURN(best), P.revPause1);
                pwm = P.trackPWM;
            end
        end
    else
        if t - tStart >= P.timeout
            forced = true; done = true;
        elseif ~det
            lost = lost + 1;
            if lost >= P.lostWin
                turn = false;
                [th, om, t, mdir] = setDir(P, phi, blocks, offs, th, om, t, mdir, 1, P.revPause1);
                pwm = P.spinPWM;
            end
        else
            lost = 0;
            if aligned
                done = true;
            else
                d = TURN(best);
                if d ~= 0 && d ~= mdir
                    [th, om, t, mdir] = setDir(P, phi, blocks, offs, th, om, t, mdir, d, P.revPause1);
                    pwm = P.trackPWM;
                end
            end
        end
    end
end
if ~done || isnan(tFirst), tt = NaN; err = NaN; return; end
tt = t - tFirst;
err = settle(P, phi, th, om);
end

%% =============== V2: spin_detect_clock (10 ms slices, PID) ===============
function [tt, forced, err] = runV2(P, phi, blocks)
offs = [-120 120 0];  W = [-1 1 0];
th = 0; om = omg(P.spinPWM); t = 0; mdir = 1; pwm = P.spinPWM; forced = false;
ring = zeros(0,3);
while true                                            % SCAN: armed from power-up
    [c, th, om, t] = slice(P, phi, blocks, offs, th, om, t, mdir*omg(pwm));
    ring = [ring; c]; if size(ring,1) > 10, ring(1,:) = []; end
    if size(ring,1) == 10 && max(sum(ring,1)) >= P.K, break; end
    if t > 20, tt = NaN; err = NaN; return; end
end
pos = posOf(sum(ring,1), W, P.K);                     % startTurn: first direction from pos
dir0 = 1; if pos < 0, dir0 = -1; end
[th, om, t, mdir] = setDir(P, phi, blocks, offs, th, om, t, mdir, dir0, P.revPause2);
pwm = clampv(P.Kp*abs(pos), P.minPWM, P.maxPWM);
I = 0; D = 0; ePrev = pos; stopRun = 0; crossed = false; lastSign = 0; tStart = t;
while true
    [c, th, om, t] = slice(P, phi, blocks, offs, th, om, t, mdir*omg(pwm));
    ring = [ring; c]; ring(1,:) = [];
    el = t - tStart;
    if el >= P.force, forced = true; break; end       % hard limit: always ends in a stop
    c50 = sum(ring(end-4:end,:), 1);
    if max(c50) < P.trackThr, stopRun = 0; continue; end
    pos = posOf(c50, W, P.trackThr);
    dead = P.deadB; if el < P.relax, dead = P.deadA + (P.deadB-P.deadA)*el/P.relax; end
    sg = 0; if abs(pos) >= P.signMin, sg = sign(pos); end
    if sg ~= 0
        if lastSign ~= 0 && sg ~= lastSign, crossed = true; end
        lastSign = sg;
    end
    centred = abs(pos) <= dead || crossed;
    if centred, stopRun = stopRun + 1; else, stopRun = 0; end
    if stopRun >= P.stopSlices, break; end
    dt = P.SL;  e = pos;
    if (e > 0) ~= (ePrev > 0), I = 0; end
    I = clampv(I + e*dt, -P.Imax, P.Imax);
    D = 0.8*D + 0.2*(e - ePrev)/dt;  ePrev = e;
    u = P.Kp*e + P.Ki*I + P.Kd*D;
    dir = mdir; if abs(pos) >= P.hyst, if pos > 0, dir = 1; else, dir = -1; end, end
    pwmNew = clampv(abs(u), P.minPWM, P.maxPWM);
    if centred && pwmNew > P.minPWM, pwmNew = P.minPWM; end
    if dir ~= mdir
        [th, om, t, mdir] = setDir(P, phi, blocks, offs, th, om, t, mdir, dir, P.revPause2);
    end
    pwm = pwmNew;
end
tt = el;
err = settle(P, phi, th, om);
end

%% =============== shared model pieces ===============
function [c, th, om, t] = slice(P, phi, blocks, offs, th, om, t, tg)
[th2, om2] = adv(P, th, om, tg);
on = blocks(min(floor(t/0.1)+1, numel(blocks)));
c = zeros(1,3);
for i = 1:3
    lam = P.peak/10*0.5*(gain(phi-(th+offs(i))) + gain(phi-(th2+offs(i))))*on + P.bg;
    c(i) = poisson_(lam);
end
th = th2; om = om2; t = t + P.SL;
end

function [th, om, t, mdir] = setDir(P, phi, blocks, offs, th, om, t, mdir, newdir, pauseS)
% driveMotor(): reversing while running forces a stop-pause (no counting) before the new direction
if newdir ~= mdir && abs(om) > 0.5
    for k = 1:round(pauseS/P.SL), [th, om] = adv(P, th, om, 0); t = t + P.SL; end
end
mdir = newdir;
end

function err = settle(P, phi, th, om)               % motor off, coast to rest
for k = 1:100
    [th, om] = adv(P, th, om, 0);
    if abs(om) < 0.5, break; end
end
err = abs(wrapd(phi - th));
end

function [th2, om2] = adv(P, th, om, tg)            % first-order motor response over one slice
e = exp(-P.SL/P.tau);
th2 = th + tg*P.SL + (om - tg)*P.tau*(1-e);
om2 = tg + (om - tg)*e;
end

function w = omg(pwm), w = max(0, (pwm-60)*1.29); end             % ASSUMED deg/s per PWM
function a = wrapd(a), a = mod(a+180,360) - 180; end
function g = gain(d), d = abs(wrapd(d)); if d < 45, g = cos(pi/2*d/45); else, g = 0; end, end
function x = clampv(x, lo, hi), x = max(lo, min(hi, x)); end
function p = posOf(c, W, thr)                                    % centroid of the sensor weights
use = c >= thr;  s = sum(c(use));
if s > 0, p = sum(W(use).*c(use))/s; else, p = 0; end
end
function k = poisson_(lam)                                       % Knuth; no toolbox needed
L = exp(-lam); k = 0; p = 1;
while true
    p = p*rand;  if p <= L, break; end
    k = k + 1;
end
end
function q = pct(x, p)                                           % linear-interpolated percentile
x = sort(x(:)); n = numel(x); pos = 1 + (n-1)*p/100;
lo = floor(pos); hi = ceil(pos); q = x(lo) + (pos-lo)*(x(hi)-x(lo));
end
function [t, df, p] = welch(a, b)                                % Welch t-test, two-sided, base MATLAB
n1 = numel(a); n2 = numel(b); v1 = var(a)/n1; v2 = var(b)/n2;
t = (mean(a)-mean(b))/sqrt(v1+v2);
df = (v1+v2)^2/(v1^2/(n1-1) + v2^2/(n2-1));
p = betainc(df/(df+t^2), df/2, 0.5);
end
