import { ComponentFixture, TestBed } from '@angular/core/testing';
import { provideHttpClient } from '@angular/common/http';
import { provideRouter } from '@angular/router';
import { CommonModule } from '@angular/common';

import { SoloChanceComponent } from './solo-chance.component';
import { SystemApiService } from 'src/app/services/system.service';
import { LoadingService } from 'src/app/services/loading.service';
import { DiffSuffixPipe } from 'src/app/pipes/diff-suffix.pipe';
import { HashSuffixPipe } from 'src/app/pipes/hash-suffix.pipe';
import { TooltipIconComponent } from '../tooltip-icon/tooltip-icon.component';
import { TooltipTextIconComponent } from '../tooltip-text-icon/tooltip-text-icon.component';

describe('SoloChanceComponent', () => {
  let component: SoloChanceComponent;
  let fixture: ComponentFixture<SoloChanceComponent>;

  beforeEach(async () => {
    await TestBed.configureTestingModule({
      declarations: [SoloChanceComponent, TooltipIconComponent, TooltipTextIconComponent],
      imports: [CommonModule, DiffSuffixPipe, HashSuffixPipe],
      providers: [
        provideRouter([]),
        provideHttpClient(),
        SystemApiService,
        LoadingService
      ]
    }).compileComponents();

    fixture = TestBed.createComponent(SoloChanceComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
