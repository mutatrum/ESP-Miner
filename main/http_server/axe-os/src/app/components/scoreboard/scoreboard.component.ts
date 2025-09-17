import { Component, OnInit, OnDestroy } from '@angular/core';
import { Observable, Subject, switchMap, shareReplay, timer, takeUntil, finalize } from 'rxjs';
import { SystemService } from 'src/app/services/system.service';
import { LoadingService } from 'src/app/services/loading.service';
import { ISystemScoreboardEntry } from 'src/models/ISystemScoreboard';

@Component({
  selector: 'app-scoreboard',
  templateUrl: './scoreboard.component.html',
  styleUrls: ['./scoreboard.component.scss']
})
export class ScoreboardComponent implements OnInit, OnDestroy {
  public scoreboard$: Observable<ISystemScoreboardEntry[]>;

  private destroy$ = new Subject<void>();

  constructor(
    private systemService: SystemService,
    private loadingService: LoadingService,
  ) {
    this.scoreboard$ = timer(0, 5000).pipe(
      switchMap(() => this.systemService.getScoreboard().pipe(
        finalize(() => this.loadingService.loading$.next(false))
      )),
      shareReplay({refCount: true, bufferSize: 1}),
      takeUntil(this.destroy$)
    );
  }

  ngOnInit() {
    this.loadingService.loading$.next(true);
  }

  ngOnDestroy() {
    this.destroy$.next();
    this.destroy$.complete();
  }
}
