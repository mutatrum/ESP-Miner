import { Injectable } from '@angular/core';
import { BehaviorSubject, Observable, Subject, EMPTY, timer, merge } from 'rxjs';
import { catchError, retry, share, tap, switchMap, startWith, scan, shareReplay, map, timeout } from 'rxjs/operators';
import { webSocket, WebSocketSubject } from 'rxjs/webSocket';
import { SystemInfo as ISystemInfo } from 'src/app/generated';
import { SystemApiService } from './system.service';

@Injectable({
  providedIn: 'root'
})
export class LiveDataService {
  private socket$: WebSocketSubject<any> | null = null;
  private updates$ = new Subject<Partial<ISystemInfo>>();
  
  // Shared info stream for the whole app
  public readonly info$: Observable<ISystemInfo>;
  
  // Connection status for the UI
  private connectedSubject = new BehaviorSubject<boolean>(false);
  public connected$ = this.connectedSubject.asObservable();

  constructor(
    private systemService: SystemApiService
  ) {
    // 1. Initial fetch from HTTP
    const initialInfo$ = this.systemService.getInfo().pipe(
      catchError(err => {
        console.error('Initial info fetch failed', err);
        return EMPTY;
      })
    );

    // 2. Continuous updates from WebSocket
    const socketUpdates$ = this.connect().pipe(
      catchError(() => EMPTY)
    );

    // 3. Periodic polling fallback (only if socket is not connected)
    const fallbackPolling$ = timer(5000, 5000).pipe(
      switchMap(() => {
        if (this.connectedSubject.value) return EMPTY;
        return this.systemService.getInfo();
      }),
      catchError(() => EMPTY)
    );

    // 4. Combined stream: Initial -> (WS updates OR Fallback)
    this.info$ = initialInfo$.pipe(
      switchMap(initial => 
        merge(
          socketUpdates$.pipe(switchMap(() => EMPTY)), // Just to trigger/maintain connection
          this.updates$, // Delta updates from WebSocket
          fallbackPolling$ // Periodic full updates as fallback
        ).pipe(
          startWith(initial),
          scan((acc, curr) => ({ ...acc, ...curr }), {} as ISystemInfo)
        )
      ),
      shareReplay(1)
    );
  }

  private connect(): Observable<any> {
    if (this.socket$ || !window.location.host) {
      return EMPTY;
    }

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const host = window.location.host;
    const url = `${protocol}//${host}/api/ws/live`;

    this.socket$ = webSocket({
      url,
      openObserver: {
        next: () => {
          console.log('Live WebSocket connected');
          this.connectedSubject.next(true);
        }
      },
      closeObserver: {
        next: () => {
          console.log('Live WebSocket disconnected');
          this.connectedSubject.next(false);
          this.socket$ = null;
        }
      }
    });

    return this.socket$.pipe(
      timeout(5000),
      tap(msg => {
        if (msg.event === 'update' && msg.data) {
          this.updates$.next(msg.data);
        }
      }),
      retry({ delay: 5000 }),
      share()
    );
  }
}
