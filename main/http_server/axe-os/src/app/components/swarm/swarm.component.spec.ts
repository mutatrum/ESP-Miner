import { ComponentFixture, TestBed } from '@angular/core/testing';
import { SwarmComponent } from './swarm.component';
import { ModalComponent } from '../modal/modal.component';
import { ReactiveFormsModule } from '@angular/forms';
import { provideHttpClient, withXhr } from '@angular/common/http';
import { ToastService } from 'src/app/services/toast.service';

describe('SwarmComponent', () => {
  let component: SwarmComponent;
  let fixture: ComponentFixture<SwarmComponent>;

  beforeEach(() => {
    TestBed.configureTestingModule({
      imports: [ReactiveFormsModule, SwarmComponent, ModalComponent],
      providers: [
        provideHttpClient(withXhr()),
        { provide: ToastService, useValue: { success: jasmine.createSpy(), error: jasmine.createSpy(), warning: jasmine.createSpy() } }
      ]
    });
    fixture = TestBed.createComponent(SwarmComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
