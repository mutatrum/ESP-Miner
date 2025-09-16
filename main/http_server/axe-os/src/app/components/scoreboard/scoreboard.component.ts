import { Component, OnDestroy } from '@angular/core';
import { Observable, Subject, switchMap, shareReplay, timer } from 'rxjs';
import { SystemService } from 'src/app/services/system.service';
import { ISystemScoreboardEntry } from 'src/models/ISystemScoreboard';

@Component({
  selector: 'app-scoreboard',
  templateUrl: './scoreboard.component.html',
  styleUrls: ['./scoreboard.component.scss']
})
export class ScoreboardComponent implements OnDestroy {
  public scoreboard$: Observable<ISystemScoreboardEntry[]>;

  private destroy$ = new Subject<void>();

  constructor(
    private systemService: SystemService,
  ) {
    this.scoreboard$ = timer(0, 2000).pipe(
      switchMap(() => this.systemService.getScoreboard()),
      shareReplay(1)
    );

  }

  ngOnDestroy() {
    this.destroy$.next();
    this.destroy$.complete();
  }
}
