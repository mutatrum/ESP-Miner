import { Component, ChangeDetectionStrategy } from '@angular/core';
import { RouterOutlet } from '@angular/router';
import { NgClass } from '@angular/common';
import { LayoutService } from './layout/service/app.layout.service';
import { SnowflakesComponent } from './components/snowflakes/snowflakes.component';
import { ToastService } from './services/toast.service';

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.scss'],
  changeDetection: ChangeDetectionStrategy.Eager,
  imports: [RouterOutlet, SnowflakesComponent, NgClass],
  standalone: true
})
export class AppComponent {
  constructor(
    public layoutService: LayoutService,
    public toastService: ToastService
  ) { }
}
